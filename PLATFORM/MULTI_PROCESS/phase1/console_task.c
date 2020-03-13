/************************************************************************/
/*	 Copyright (C) 2003 Foundry Networks								*/
/*	 Unpublished - rights reserved under the Copyright Laws of the		*/
/*	 United States.	 Use, duplication, or disclosure by the				*/
/*	 Government is subject to restrictions as set forth in				*/
/*	 subparagraph (c)(1)(ii) of the Rights in Technical Data and		*/
/*	 Computer Software clause at 252.227-7013.							*/
/*																		*/
/************************************************************************/
/*
 * console_task.c
 *
 * Console Tasks
 *
 * Author: Marc Lavine
 */
#include "sw.h"
#include "cli.h"
#include "cli_sys.h"
#include "cli_impl.h"
#include "cu_session_sys.h"
#include "parser.h"
#include "con_sys.h"
#include "con_impl.h"
#include "cu_sys.h"
#include "fifo.h"
#include "aaa.h"
#include "cmds.h"
#include "vintuart.h"
#include "sw_inf_fi_semaphore_lib.h"
#include "chassis.h"


extern ITC_CONTEXT appl_itc_context;
extern UINT8 config_term_user_count;
extern UINT8 reset_first_few_queue;
CONSOLE_CLASS console;

static UINT32 g_console_session_time_in_secs=0;
static UINT32 g_console_idle_timeout=0;

#define IO_POLL_MSECS	50
#define CONSOLE_INPUT_BUFFER_SIZE	(64*1024)
#define CONSOLE_OUTPUT_BUFFER_SIZE	(64*1024)

#define IS_IDLE_CHECKABLE_SESSION(s) (IS_CONSOLE_CDBS_INDEX(s) \
	|| (IS_RCONSOLE_SESSION(s) && rconsole_server_is_session_valid(s)))

static ITC_MSG_DEFINITION console_msg_defs[] =
{
	ITC_DEFINE_MESSAGE(MSG_TYPE_CONSOLE_INPUT, ITC_MSGFLAG_NO_RESPONSE, 0, 0),
	ITC_DEFINE_MESSAGE(MSG_TYPE_TEXT_MSG, ITC_MSGFLAG_NO_RESPONSE, 0, 0),
	ITC_DEFINE_MESSAGE(MSG_TYPE_KILL_CONSOLE, ITC_MSGFLAG_NO_RESPONSE, 0, 0),
	ITC_DEFINE_MESSAGE(MSG_TYPE_RESET_CONSOLE, ITC_MSGFLAG_NO_RESPONSE, 0, 0),
};

static void console_handle_input(void *request_ptr, void *response_ptr);
static void console_handle_uprintf_output(void *request_ptr, void *response_ptr);
static void console_session_reset(void *request_ptr, void *response_ptr);
static void console_session_close(void *request_ptr, void *response_ptr);
void console_cancel_session_timer();
static void console_task_go_online();

static ITC_CALLBACK_REGISTRATION console_callback_registrations[] =
{
	{MSG_TYPE_CONSOLE_INPUT, console_handle_input},
	{MSG_TYPE_KILL_CONSOLE, console_session_reset},
	{MSG_TYPE_RESET_CONSOLE, console_session_close},
};

static CHAR_FIFO *input_fifo_ptr;
static CHAR_FIFO *output_fifo_ptr;
static BOOLEAN expand_next_newline = TRUE;
static BOOLEAN input_notification_pending = FALSE;
static BOOLEAN console_running = FALSE;

typedef struct
{
	TimerId timer_id;
	ITC_CONTEXT itc_context;
	void (*callback)(UINT32 param);
	TIMER_TYPE type;
	UINT32 timeout_ms;
	UINT32 param;
} TIMER_DATA;

static hashFastGeneric *timer_entries;

static UINT32 get_timer_entry_key(UINT32 value)
{
	TIMER_DATA *timer_data_ptr = (TIMER_DATA *) value;

	return (UINT32) &timer_data_ptr->timer_id;
}

static int compare_key_and_timer_entry(UINT32 key, UINT32 value)
{
	TimerId *timer_id_ptr = (TimerId *) key;
	TIMER_DATA *timer_data_ptr = (TIMER_DATA *) value;
	INT64 diff;

	diff = (INT64) *timer_id_ptr - (INT64) timer_data_ptr->timer_id;

	return (diff < 0) ? -1 : ((diff > 0) ? 1 : 0);
}

static UINT32 get_timer_entry_hash(UINT32 key)
{
	TimerId *timer_id_ptr = (TimerId *) key;

	return (UINT32) *timer_id_ptr;
}

static void timer_callback_wrapper(void *request_ptr, void *response_ptr)
{
	itc_msg_timer_timeout *timeout_msg = (itc_msg_timer_timeout *) request_ptr;
	TIMER_DATA *timer_ptr;

	if (hashFastGenericGet(timer_entries, (UINT32) &timeout_msg->timer_id,
						   (UINT32 *) &timer_ptr))
	{
		(*timer_ptr->callback)(timer_ptr->param);

		if (timer_ptr->type == ONCE_TIMER)
		{
			if (!hashFastGenericDelete(timer_entries,
									   (UINT32) &timer_ptr->timer_id, NULL))
			{
				uprintf("Warning: page mode timer_callback_wrapper failed to"
						" remove timer entry\n");
			}

			os_free(timer_ptr);
		}
	}
}

SV_TIMER_TOKEN_T cu_sv_set_timer(UINT32 ticks, TIMER_TYPE type,
								 void (*callback)(UINT32), UINT32 param)
{
	TIMER_ERROR timer_error;
	TIMER_DATA *timer_ptr;
	TimerId timer_id = 0;
	int hash_inserted;

	if (g_cu_session >= MAX_CU_SESSIONS)
		return NULL;

	timer_ptr = (TIMER_DATA *) os_malloc(sizeof(*timer_ptr));
	if (timer_ptr == NULL)
		return NULL;

	timer_ptr->itc_context = cu_get_current_itc_context();
	/* If itc_context is not available for the task, which is setting the timer, 
	 * use appl itc_context.
	 */
	if (!timer_ptr->itc_context)
		timer_ptr->itc_context = appl_itc_context;

	timer_ptr->callback = callback;
	timer_ptr->type = type;
	timer_ptr->param = param;
	timer_ptr->timeout_ms = ticks * 1000 / sys_get_timeticks_per_second();
	timer_ptr->timer_id = 0;

	timer_error = StartTimer2(timer_ptr->itc_context,
		ITC_MSG_TYPE_PAGE_MODE_TIMER, timer_ptr->timeout_ms, type, 0,
		&timer_ptr->timer_id);

	if (timer_error != TIMER_OK)
		goto cleanup;

	hash_inserted = hashFastGenericInsert(timer_entries,
		(UINT32) &timer_ptr->timer_id, (UINT32) timer_ptr);
	if (!hash_inserted)
		goto cleanup;

	timer_id = timer_ptr->timer_id;

cleanup:
	if (timer_id == 0)
	{
		if (timer_ptr->timer_id != 0)
			CancelTimer2(timer_ptr->itc_context, timer_ptr->timer_id);

		os_free(timer_ptr);
	}

	return (SV_TIMER_TOKEN_T) timer_id;
}

TIMER_ERROR cu_sv_cancel_timer(SV_TIMER_TOKEN_T timer)
{
	TIMER_ERROR timer_error;
	TIMER_DATA *timer_ptr;

	if (hashFastGenericDelete(timer_entries, (UINT32) &timer,
							  (UINT32 *) &timer_ptr))
	{
		timer_error = CancelTimer2(timer_ptr->itc_context,
								   timer_ptr->timer_id);
		os_free(timer_ptr);
	}
	else
		timer_error = TIMER_ERROR_EXIST;

	return timer_error;
}

/*
 * Initialize the data structures for the specified CLI session .
 */
BOOLEAN cli_session_init_state(UINT32 session, ITC_CONTEXT context)
{
	TIMER_ERROR timer_error;

	timer_error = RegisterTimerCallback2(context,
		ITC_MSG_TYPE_PAGE_MODE_TIMER, timer_callback_wrapper);
	return (timer_error == TIMER_OK);
}

BOOLEAN cli_session_reset_state(UINT32 session, ITC_CONTEXT context)
{
	TIMER_ERROR timer_error;

	timer_error = DeregisterTimerCallback2(context,
		ITC_MSG_TYPE_PAGE_MODE_TIMER, timer_callback_wrapper);
	return (timer_error == TIMER_OK);
}

/* Console Class access/modification functions. */
void console_set_idle_timeout_config(unsigned int value)
{
	g_console_idle_timeout = value;

	if (value == 0)
	{
		int session;
		for (session = 0; session < MAX_IO_CB; ++session)
		{
			if (!IS_IDLE_CHECKABLE_SESSION(session))
				continue;
			io_cb[session].idle_timer = 0;
		}
	}

}

void console_reset_idle_timeout_current(void)
{
	console.idle_timeout_current = console.idle_timeout_config;
	console.idle_timer_counter = 0;
}

unsigned int console_get_idle_timeout_config(void)
{
	return console.idle_timeout_config;
}

void console_set_outbound_session_id(unsigned int value)
{
	console.outb_session_id = value;
}
unsigned int console_get_outbound_session_id()
{
	return console.outb_session_id;
}

static void console_session_close(void *request_ptr, void *response_ptr)
{
	
	itc_msg_reset_console *myMsgP = request_ptr;
	UINT32 session = myMsgP->session;

	if (!aaa_close_exec_session((UINT8)session, cli_aaa_accounting_callback, session))
	print_prelogin_message_on_console(session);

	return;
}


static void console_session_reset(void *request_ptr, void *response_ptr)
{
	itc_msg_kill_console *myMsgP = request_ptr;
	unsigned int mode;
	mode = parser_aaa_get_mode(CONSOLE_SESSION);

	console_cancel_session_timer();
	console_reset_idle_timeout_current();
	if (cu_aaa_is_login_authen_enabled_for_console() && is_mgmt_active())
	{
		if (aaa.session[CONSOLE_SESSION].login_attempts == 0)
			aaa_close_exec_session(CONSOLE_SESSION, cli_aaa_accounting_callback, CONSOLE_SESSION);
		if (mode != CONSOLE_LOGIN) {
			print_prelogin_message_on_console(CONSOLE_SESSION);
		}
	}
	else
	{
		if (myMsgP->subType != EVENT_ID_MP_SWITCHOVER_START) // 79024 fix
		{
			int log_enabled = 1;
			struct cdb *sptr_cdb=&cdbs[CONSOLE_SESSION];

			if (mode == USER_EXEC)
			{
				//330574  if console is already in USER-EXEC mode we may not do the logging to avoid redundant logs
				log_enabled = 0;
			}
			// TR000461122: Switched the order of calling of the follownig two functions.
			// clean_up_parser resets the sptr_cdb->user_name which is needed for default_current_login_password_level
			default_current_login_password_level(sptr_cdb, log_enabled);
			clean_up_parser(CONSOLE_SESSION);
			if (mode != USER_EXEC)
			{
				uprintf_direct("\n");
				print_prompt(sptr_cdb);
			}
		}
	}
	return;
}

static ITC_ERROR cu_reset_console_session()
{
	itc_msg_kill_console request;

	request.subType = 0;
	return itc_send_request(cu_sessions[CONSOLE_SESSION].itc_context,
					 ITC_APP_CONSOLE, MSG_TYPE_KILL_CONSOLE,
					 &request, sizeof(request), ITC_PRI_HIGH, NULL, 0);
}

static ITC_ERROR cu_close_console_session(UINT32 session)
{
	itc_msg_reset_console request;

	request.session = session;
	return itc_send_request(cu_sessions[session].itc_context,
					 ITC_APP_CONSOLE, MSG_TYPE_RESET_CONSOLE,
					 &request, sizeof(request), ITC_PRI_HIGH, NULL, session);
}


void set_console_timeout_value(struct cdb *sptr_cdb)
{


    if (sptr_cdb->config_gen)
    {
        if (g_console_idle_timeout == 0)
            return;
        wr_config(sptr_cdb, "console timeout ");
        ksprintf(cu_line_buf, "%d\n", g_console_idle_timeout);
        wr_config(sptr_cdb, cu_line_buf);
    }
    else
    {
        if (!valid_integer1_value_range(sptr_cdb, 0, MAX_CONSOLE_IDLE_TIMEOUT)) {
            return;
        }
        console_set_idle_timeout_config(sptr_cdb->integer1);
    }
}

static void check_console_idle(void)
{
	ITC_ERROR itc_error;
	int session;
	struct io_port_cb *sptr_cb;

	if (g_console_idle_timeout == 0)
		return;

	// Blocking the console timeout code for SLAVE units.	
	if(STACK_AM_I_SLAVE || STACK_AM_I_PE)
        return;

	for (session = 0; session < MAX_IO_CB; ++session)
	{

		if (!IS_IDLE_CHECKABLE_SESSION(session))
			continue;

		sptr_cb = &io_cb[session];
		++sptr_cb->idle_timer;

		if (sptr_cb->idle_timer >= (2*g_console_idle_timeout))
		{
			if (get_cli_mode(&cdbs[session]) == CONSOLE_LOGIN)
			{
				continue;
			}

			if (cu_aaa_is_login_authen_enabled_for_console())
			{
				/* EXIT reason for timeout */
				cdbs[session].exit_reason = 2;
				itc_error = cu_close_console_session(session);
				if (itc_error != ITC_OK)
				{
					kprintf("Error: ITC error while resetting console session %d\n", itc_error);
				}				

			}
			else
				clean_up_parser(session);
			sptr_cb->idle_timer = 0;
		}
	}
}

ITC_ERROR console_define_itc_msgs()
{
	return itc_define_messages(console_msg_defs, ARRAY_LEN(console_msg_defs));
}

static void notify_of_pending_input(void)
{
	if (!input_notification_pending)
	{
		ITC_MSG_HEADER request;
		ITC_ERROR error;

		error = itc_send_request(cu_sessions[CONSOLE_SESSION].itc_context,
								 ITC_APP_CONSOLE, MSG_TYPE_CONSOLE_INPUT,
								 &request, sizeof(request), ITC_PRI_HIGH,
								 NULL, 0);

		if (error == ITC_OK)
			input_notification_pending = TRUE;
		else if (error != ITC_ERR_DEST_QUEUE_FULL)
			kprintf("Error: Console ITC send request error %d\n", error);
	}
}

static void drain_output_fifo()
{
	int c;

	while ((c = fifo_get(output_fifo_ptr)) >= 0)
	{
		if (c == '\n' && !is_linux_based_system())
		{
			if (expand_next_newline)
			{
				if (sys_putchar('\n') < 0)
				{
					fifo_unget(output_fifo_ptr, '\n');	/* Re-do w/expansion */
					break;
				}
			}

			if (sys_putchar('\r') < 0)
			{
				fifo_unget(output_fifo_ptr, '\n');
				expand_next_newline = FALSE;	/* Re-do without expansion */
				break;
			}

			expand_next_newline = TRUE;
		}
		else
		{
			if (sys_putchar(c) < 0)
			{
				fifo_unget(output_fifo_ptr, c);
				break;
			}
		}
	}
}

static int io_timer_callback(unsigned int param)
{
	int c;
	static int idle_time_counter = 0;

	if (is_cli_session_corrupted(CONSOLE_SESSION))
	{
		cli_reset_session(CONSOLE_SESSION);
		kprintf("\nConsole data structure corruption detected;"
				" resetting session...\n");
		print_prompt(&cdbs[CONSOLE_SESSION]);
	}

	drain_output_fifo();

	while ((c = sys_getchar(SYS_NO_WAIT)) >= 0)
	{
		idle_time_counter = 0; /* reset the ticks counter */
		console.idle_timer_counter = 0;
		if (!fifo_put(input_fifo_ptr, c))
			sys_putchar('\007');	/* Ring the bell */
	}

	/*
	 * Note: We do this even if there's no new input, in case the message
	 * queue filled up and earlier messages were not able to be sent.
	 * This also deals with re-sending notifications which are ignored due
	 * to the console task already being in the middle of processing
	 * input.
	 */
	if (!fifo_is_empty(input_fifo_ptr))
		notify_of_pending_input();

	/* update the idle time in ticks, and check every 1 minute to reset session,
	 * if no input
	 */
	if (((++idle_time_counter * IO_POLL_MSECS)/1000) >= 30)
	{
		idle_time_counter = 0;
		check_console_idle();
	}

	return 0;
}

BOOLEAN stby_cli_enabled = FALSE;

static void console_handle_input(void *request_ptr, void *response_ptr)
{
	int c;
	input_notification_pending = FALSE;

#ifndef NO_MULTI_CONFIG_VLAN
        if (g_sw_sys.vlan_range_callback_in_progress)
        {
                return; 
        }
#endif NO_MULTI_CONFIG_VLAN	

	if (g_sw_sys.parser_wait_in_progress)	/* do not process command line input for a while */
	{
		return;
	}

	while (cli_can_accept_input(CONSOLE_SESSION) && (c = fifo_get(input_fifo_ptr)) >= 0)
	{
		if(debugGlobal.stacking.debug_official & 0x8)
		{	// kklin, add this for debugging. 
			uprintf("fifo_get()=%x\n", c);
			// turn it off right away to avoid print a lot of data
			debugGlobal.stacking.debug_official&= (~0x8);
		}
		if (cli_process_input(c, CONSOLE_SESSION))
		{
			sys_yield();	/* Avoid hogging the CPU */
			break;			/* Allow other messages to be processed */
		}
	}
}

int output_fifo_ptr_sem = 0;

BOOLEAN console_output_str(char const *str, int length)
{
	BOOLEAN succeeded;

	if (console_running)
	{
		sys_take_semaphore(output_fifo_ptr_sem, 1, SYS_FOREVER);
		succeeded = fifo_put_str(output_fifo_ptr, str, length);
		if (!succeeded)
		{
			/* drain the fifo and try once more */
			drain_output_fifo();
			succeeded = fifo_put_str(output_fifo_ptr, str, length);
		}
		sys_give_semaphore(output_fifo_ptr_sem, 1);
		return succeeded;
	}
	else
	{
		output_direct_to_system_console(str, length);
		return TRUE;
	}
}

BOOLEAN is_console_up(void)
{
	return console_running;
}

/****************************************************************************/
#define CONSOLE_MSG_IO 		0x1
#define CONSOLE_MSG_RESET	0x2
 
void console_start_session_timer(int session, UINT32 a_session_time_in_secs)
{
	StartTimer(cu_sessions[CONSOLE_SESSION].itc_context, a_session_time_in_secs * 1000,
			TIMER_TYPE_ONCE, CONSOLE_MSG_RESET);
	g_console_session_time_in_secs = a_session_time_in_secs;
	return;
}

/****************************************************************************/
void console_cancel_session_timer()
{
	if (g_console_session_time_in_secs)
	{
		/* if connection was closed during before session expires,
		 * reset the session timer.
		 */
		CancelTimer(cu_sessions[CONSOLE_SESSION].itc_context,
					g_console_session_time_in_secs * 1000,
					TIMER_TYPE_ONCE, CONSOLE_MSG_RESET);
		g_console_session_time_in_secs = 0;
	}
	return;
}

/****************************************************************************/
void console_timer_callback(void *req, void *res)
{
	/* Currently this timer supports only console session timeout, so using directly.*/
	itc_msg_timer_timeout *console_timer = (itc_msg_timer_timeout *)req;
	//int timer_type = GET_TELNET_TIMER_TYPE(telnet_timer->arg);

	switch (console_timer->arg) {
#ifdef FI_TIMER_LIB_IMPL
		case CONSOLE_MSG_IO: {
			io_timer_callback(0);
			break;	
		}
#endif
		case CONSOLE_MSG_RESET: {
			cu_reset_console_session();// Coverity defect 26079
			break;
		}
		default: {
			debug_uprintf("Console Task: Timout Arg: %d unsupported\n", console_timer->arg);
			break;
		}
	}
}

void console_itc_event_switchover_start()
{
	int session_id;
	/*76080: Reset the count of users in config mode if switchover happens.
	Console is still maintained in the same state, so check its mode.*/
	config_term_user_count = 0;
	if (cdbs[CONSOLE_SESSION].mode >= CONFIG)
		config_term_user_count++;
#if 0 //More fancy fix later.
	/*Reset the SSH and telnet datastructures */
	for (session_id = 0; session_id < MAX_TELNET_SESSIONS-1; session_id++)
		cu_telnet_close_session(session_id);
	for session_id=0; session_id<MAX_SSH_SESSIONS-1; session_id++)
		cu_ssh_close_session(session_id);
#endif 0
	return;
}

void console_event_handler(void *req_ptr, void *resp_ptr)
{
	itc_msg_event *req = (itc_msg_event *)req_ptr;
	msg_event_system_init_completed *system_init_comp_event_msg;

	switch (req->event_id)
	{
	case EVENT_ID_MP_SWITCHOVER_START:
		console_itc_event_switchover_start();
		break;
	case EVENT_ID_INFRA_SYSTEM_INIT_COMPLETE:
		system_init_comp_event_msg 
			= (msg_event_system_init_completed *) ((char *)req + sizeof(itc_msg_event));
		if (system_init_comp_event_msg->bSuccess)
		{
		  kprintf("System initialization completed...console going online.\n");
		  console_task_go_online();
		}
		else
		{
		  kprintf("System initialization failed...console will not go online.\n");
		}
		break;
	default:
		kprintf("Warn:console_event_handler: unregistered event %d is received\n", req->event_id);
		break;
	}
}

/****************************************************************************/
static int console_events_register(ITC_CONTEXT context)
{
	SCP_EVENT_ERROR err=SCP_EVENT_OK;

	/* Initialize Event */
	err = RegisterEventCallback(context, console_event_handler);
	if (err != SCP_EVENT_OK)
	{
		kprintf("Error: console_task - RegisterEventCallback failed %d\n", err);
		return err;
	}
	err = RegisterEvent(context, EVENT_ID_MP_SWITCHOVER_START, SCP_EVENT_PRI_L3_NORMAL);
	if (err != SCP_EVENT_OK)
	{
		kprintf("Error: console_task - RegisterEvent failed %d\n", err);
		return err;
	}

	err = RegisterEvent(context, EVENT_ID_INFRA_SYSTEM_INIT_COMPLETE, SCP_EVENT_PRI_INFRA_HIGH);
	if (err != SCP_EVENT_OK)
	{
		kprintf("console_events_register: error - RegisterEvent failed for event %d:%s with error %d\n", 
			EVENT_ID_INFRA_SYSTEM_INIT_COMPLETE, "EVENT_ID_INFRA_SYSTEM_INIT_COMPLETE", err);
		return err;
	}

	return err;
}

extern UINT32 diag_on, alt_diag_on;
static
void console_task_go_online()
{
    ITC_CONTEXT            context = itc_get_context_using_task_handle(sys_get_current_task_handle());
	ITC_ERROR              itc_stat = ITC_OK;
	ITC_APP_ID             app_id;
	int io_timer;
	int sys_error;
    int uprintf_dest;
  
	itc_stat = itc_get_app_id(context, &app_id);
	if (itc_stat != ITC_OK)
	{
		kprintf("Error: Unable to get app ID\n");
		return;
	}

    // Print the show version output after system init only if your are not running a diag image
	if(!diag_on && !alt_diag_on)
	{	
		/* Temp fix, need to fix completely */
		uprintf_dest = g_uprintf_dest;
		g_uprintf_dest = 0;
		show_version();
		g_uprintf_dest = uprintf_dest;
	}

	/* send the warm start trap now, as the config is read now, so the
	 * snmp trap hosts must have been configured.
	 */
	send_start_trap(get_g_warm_start());
	if (cu_aaa_is_login_authen_enabled_for_console() && is_mgmt_active())
		print_prelogin_message_on_console(CONSOLE_SESSION);
	else
		print_prompt(&cdbs[CONSOLE_SESSION]);

#ifdef FI_TIMER_LIB_IMPL 
	/* Use the context Timer. */
	StartTimerDisableSkewProcessing(context, IO_POLL_MSECS, TIMER_TYPE_REPEAT, CONSOLE_MSG_IO);
#else
	io_timer = sys_create_timer("console_io", SYS_REPEAT_TIMER);
	if (io_timer == 0)
	{
		kprintf("Error: Console unable to create timer\n");
		sys_exit(1);
	}

	sys_error = sys_set_timer(io_timer, (sys_get_timeticks_per_second()* IO_POLL_MSECS)/1000, io_timer_callback, 0);
	if (sys_error != SYS_OK)
	{
		kprintf("Error: Console unable to set timer\n");
		sys_exit(1);
	}
#endif

#ifndef WIN32
//	if (g_standby_mp_red_fsm_done)
//		update_cli_mode();
#endif WIN32

	console_running = TRUE;
	// setting system status LED to green solid after console going online
	sw_system_led(SYST_LED, LED_GREEN, 0);
	cu_ntp_resolve_name_to_ip();/*To resolve NTP server/peer*/

#ifndef NO_OPTICAL_MONITOR
//	extern void tanto_om_scheduler_init();
//	tanto_om_scheduler_init();
	
	extern int recieve_thread_init();
	recieve_thread_init();

 #endif NO_OPTICAL_MONITOR

}

int console_task(TASK_PARAM *param)
{
	ITC_CONTEXT context;
	int io_timer;
	ITC_ERROR error;
	int sys_error;
	SCP_EVENT_ERROR scp_err;
	unsigned long ckrv;
#if defined (PV_CHANGES)
	int io_timer_id;
#endif

 	// wait for signal to start the initialization.
        task_init_complete(param, 1);
	
	input_fifo_ptr = fifo_create(CONSOLE_INPUT_BUFFER_SIZE);
	if (input_fifo_ptr == NULL)
	{
		kprintf("Error: console_task: input fifo_create failed\n");
		sys_exit(1);
	}

	output_fifo_ptr = fifo_create(CONSOLE_OUTPUT_BUFFER_SIZE);
	if (output_fifo_ptr == NULL)
	{
		kprintf("Error: console_task: ouput fifo_create failed\n");
		sys_exit(1);
	}

	output_fifo_ptr_sem = sys_create_semaphore("output_fifo_ptr_sem", 1);
#ifdef FI_LINUX
	sys_give_semaphore(output_fifo_ptr_sem, 1);
#endif

	if ((error = itc_init_app(CONSOLE_TASK_NAME, ITC_APP_CONSOLE, &context,
							  CONSOLE_MESSAGE_QUEUE_SIZE)) != ITC_OK)
	{
		kprintf("Error: console_task itc_init_app() failed\n");
		sys_exit(1);
	}

	timer_entries = hashFastGenericCreate2(17, get_timer_entry_key,
		compare_key_and_timer_entry, get_timer_entry_hash,
		&global_mem_allocator);

	/* stage 1 init complete */
	task_init_complete(param, 1);

	/* Register message request callbacks */
	error = itc_register_callbacks(context,
								 console_callback_registrations,
								 ARRAY_LEN(console_callback_registrations));
	if (error != ITC_OK)
	{
		kprintf("Error: console_task: itc_register_callbacks failed (%d)\n",
				error);
		sys_exit(1);
	}

	error = cli_register_itc_callbacks(context);
	if (error != ITC_OK)
	{
		kprintf("Error: console_task: cli_register_itc_callbacks"
				" failed (error=%d)\n", error);
		sys_exit(1);
	}

	error = RegisterTimerCallback(context, console_timer_callback);
	if (error != TIMER_OK)
	{
		kprintf("Error: console_task: RegisterTimerCallback"
				" failed (error=%d)\n", error);
		sys_exit(1);
	}

	/* Initialize Event */
	if ((scp_err = console_events_register(context)) != SCP_EVENT_OK)
	{
		sys_exit(1);
	}

	if (!cu_session_init_state(CONSOLE_SESSION, context) ||
		itc_set_context_cu_session(context, CONSOLE_SESSION) != ITC_OK)
	{
		kprintf("Error: console_task: CU session setup failed\n");
		sys_exit(1);
	}

	/* stage 2 init complete */
	task_init_complete(param, 1);

	if (!cli_session_init_state(CONSOLE_SESSION, context))
	{
		kprintf("Error: Unable to initialize console session\n");
		sys_exit(1);
	}
//	cu_pre_init();

	/* Reads the configuration file, load into the system and set up  the configuration in all the tasks. 
	 *  With this, the system init is complete.
	 */
	 // 
	if(!STACK_BOOTUP_AS_SLAVE && !STACK_AM_I_PE)	/* BUG:91390 */ // superX safe.
	{	 
		filock_take();
		init_runConfig_from_startConfig();
		filock_give();
	}

//	cu_post_init();
//	g_sw_sys.init_in_progress = FALSE;
//	cu_post_init_in_progress();

	/* TR000484745: This was needed for FIPS specific.During bootup time, Public key was parsed before 
	 * config file gets parsed and hashing gets mismatched. so moving here to Parse Public key after 
	 * config file gets parsed. In Future, if we know the right place this can be moved.
	 */
	memset(&ssh.client_pub_keys, 0, sizeof(ssh.client_pub_keys));
	ssh_load_client_pub_key();
	reset_first_few_queue = 1;
	task_init_complete(param, 0);

 	handle_elapse_time(3); // kklin, add this to track task activity
	itc_process_msgs(context, ITC_TIMEOUT_FOREVER);
	return 0;
}

void console_output_purge(void)
{
   if (output_fifo_ptr)
       fifo_clear(output_fifo_ptr);
}

void console_reset_cleanup(void)
{
	if (output_fifo_ptr)
		io_timer_callback(0);
}
