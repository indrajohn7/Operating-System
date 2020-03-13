#include "sw.h"
#include "sw_sxr_red.h"
#include "taskdefs.h"
#include "itc_sys.h"
#include "timer.h"
#include "cu_snmp.h"
#include "keygen.h"
#include "cli_impl.h"
#include "sw_inf_fi_timer.h"
#include "l2_cu.h"
#include "syslog.h"
#include "fi_Task_Param_Defs.h"
#include "plm.h"

#ifndef CONSOLE_MESSAGE_QUEUE_SIZE
#define CONSOLE_MESSAGE_QUEUE_SIZE	16384
#endif

/* Prateek - Increasing the stack size to fix linux stack overflows
 * need to re look at individual task needs later */

extern TASK_CONFIG appl_tasks[];
extern unsigned int fi_Task_Get_Total_Count();

int g_task_keygen;

extern void ipsec_get_mem_req(INT32 *sptr_dy_mp, INT32 *sptr_sh_mp,
                                 INT32 *sptr_dy_lp, INT32 *sptr_sh_lp,
                                 BOOL configtime);

extern void ike_get_mem_req(INT32 *sptr_dy_mp, INT32 *sptr_sh_mp,
                                 INT32 *sptr_dy_lp, INT32 *sptr_sh_lp,
                                 BOOL configtime);

#ifdef SR_SWITCH_ROUTER
extern void ospf6_get_mem_req(INT32 *sptr_dy_mp, INT32 *sptr_sh_mp,
                       INT32 *sptr_dy_lp, INT32 *sptr_sh_lp,
                       BOOL configtime);
extern void ripng_get_mem_req(INT32 *sptr_dy_mp, INT32 *sptr_sh_mp,
                       INT32 *sptr_dy_lp, INT32 *sptr_sh_lp,
                       BOOL configtime);

#ifdef __DHCP6_AGENT__
extern void dhcp6_get_mem_req(INT32 *sptr_dy_mp, INT32 *sptr_sh_mp,
                       INT32 *sptr_dy_lp, INT32 *sptr_sh_lp,
                       BOOL configtime);
#endif __DHCP6_AGENT__

#endif SR_SWITCH_ROUTER

/* ERSPAN */
extern void event_erspan_route_status(ITC_MSG_TYPE status, RTM_IPV4_CLIENT_REQ *param);

int appl_send_vlan_update_evt_req(VLAN_ENTRY_UPDATE_REQ_T *evt_req);

extern enum BOOLEAN dhcpc_is_printf_group_debug_set(void);

ITC_CONTEXT appl_itc_context;
int         g_timer_task;
int         g_all_task_initialization_done = 0;

#ifdef __IP_MULTICAST__
#ifdef FASTIRON_MCAST_FWD_TASK_DISABLE
void mcastlp_ipc_itc_msg_callback(void *req, void *res);
void mcast6lp_ipc_itc_msg_callback(void *req, void *res);
#endif
#endif

static void handle_itc_uprintf_output_msg(void *request_ptr, void *response_ptr);
void appl_main_timer_callback(void *req, void *res);
void appl_itc_l2_config_callback(void *req, void *res); //L2 CLI

#ifdef FASTIRON
static void appl_itc_rtm_route_not_avail (void *req_ptr, void *resp_ptr);
static void appl_itc_rtm_route_avail (void *req_ptr, void *resp_ptr);
static void appl_itc_rtm_route_update (void *req_ptr, void *resp_ptr);
#endif
#ifdef __FI_IPSEC_IKE__
extern void ipsec_mm_appl_task_itc_msg_callback(void *req, void *res);
#endif

void appl_itc_vlan_entry_update_callback(void *req_ptr, void *resp_ptr);

ITC_MSG_DEFINITION appl_task_itc_msg_defs[] = 
{
#ifdef __IP_MULTICAST__
#ifdef FASTIRON_MCAST_FWD_TASK_DISABLE
	ITC_DEFINE_MESSAGE(ITC_TYPE_MCASTLP_IPC_MSG, ITC_MSGFLAG_NO_RESPONSE, 0, 0),
	ITC_DEFINE_MESSAGE(ITC_TYPE_MCAST6LP_IPC_MSG, ITC_MSGFLAG_NO_RESPONSE, 0, 0),
#endif
#endif
	ITC_DEFINE_MESSAGE(ITC_MSG_TYPE_ROUTE_FILTER_CHANGE,ITC_MSGFLAG_NO_RESPONSE,0,	15000),
	ITC_DEFINE_MESSAGE( ITC_MSG_TYPE_ROUTE_FILTER_MODIFY,ITC_MSGFLAG_NO_RESPONSE,0,	15000),
	ITC_DEFINE_MESSAGE( ITC_MSG_TYPE_ROUTE_FILTER_DELETE,ITC_MSGFLAG_NO_RESPONSE,0,	15000),
	ITC_DEFINE_MESSAGE( ITC_MSG_TYPE_CLEAR_FILTER_UPDATE_DELAY_TIME,ITC_MSGFLAG_NO_RESPONSE,0,	15000),
	ITC_DEFINE_MESSAGE(ITC_MSG_TYPE_L2_CONFIG_MSG, 0, sizeof(L2_ITC_GENERIC_RESP), -1), //L2 CLI
#ifdef __FI_IPSEC_IKE__
	ITC_DEFINE_MESSAGE(ITC_MSG_TYPE_IPSEC_APPL_TASK_MSG,ITC_MSGFLAG_NO_RESPONSE, 0, 15000 ),
#endif
    ITC_DEFINE_MESSAGE(ITC_MSG_TYPE_VLAN_ENTRY_UPDATE, ITC_MSGFLAG_NO_RESPONSE, 0, 15000),
};

ITC_CALLBACK_REGISTRATION appl_task_callback_registrations[] =
{
#ifdef __IP_MULTICAST__
#ifdef FASTIRON_MCAST_FWD_TASK_DISABLE
#ifdef SR_SWITCH_ROUTER
	{ITC_TYPE_MCASTLP_IPC_MSG, mcastlp_ipc_itc_msg_callback},
	{ITC_TYPE_MCAST6LP_IPC_MSG, mcast6lp_ipc_itc_msg_callback},
#endif SR_SWITCH_ROUTER
#endif
#endif
	{ITC_MSG_TYPE_UPRINTF_OUTPUT, handle_itc_uprintf_output_msg},
	{ITC_MSG_TYPE_L2_CONFIG_MSG, appl_itc_l2_config_callback}, // Maocheng++: appl task's L2 cli,  {ITC_MSG_TYPE_CLUSTER_CONFIGURATION, clustermgr_itc_configuration},
#ifdef SR_SWITCH_ROUTER
	{RTM_CLIENT_ROUTE_NOT_AVAIL, appl_itc_rtm_route_not_avail},
	{RTM_CLIENT_ROUTE_AVAIL, appl_itc_rtm_route_avail},
	{RTM_CLIENT_ROUTE_UPDATE, appl_itc_rtm_route_update},
#endif
	// Dy_sync over ITC related defines:
	dy_sync_itc_slave_callbacks,
#ifdef __FI_IPSEC_IKE__
	{ITC_MSG_TYPE_IPSEC_APPL_TASK_MSG, ipsec_mm_appl_task_itc_msg_callback},
#endif
    {ITC_MSG_TYPE_VLAN_ENTRY_UPDATE, appl_itc_vlan_entry_update_callback},
};

ITC_ERROR appl_define_itc_msgs(void)
{
	ITC_ERROR error;
	
	if ((error = itc_define_messages(appl_task_itc_msg_defs, ARRAY_LEN(appl_task_itc_msg_defs))) != ITC_OK)
	{
		kprintf("error - appl_task: itc_define_message() failed\n");
		return error;
	}

	dy_sync_slave_init_itc_message_types();
	dy_sync_mgmt_init_itc_message_types();

	return ITC_OK;
}

ITC_ERROR appl_init_itc_app(void)
{
	ITC_ERROR   error;
	ITC_CONTEXT context;
        int         itcMsgQueLen = adm_TM_Calculate_ItcMsg_Que_Max_Len();

	if ((error = itc_init_app(APPL_TASK_NAME, ITC_APP_APPL, &context, CONSOLE_MESSAGE_QUEUE_SIZE)) != ITC_OK)
	{
		kprintf("error - appl_task: itc_init_app() failed\n");
		return error;
	}

        /****************************************************************************
        **** APPL-Task To Other-FI-Task ITC-Message Retry-Send-Queue-Size Setting
        *****************************************************************************/
        error = itc_set_context_send_queue_size(
                                context,
                                itcMsgQueLen);
        if (error != ITC_OK)
        {
            kprintf("APPL_TASK_FAILED: ITC-SET-CONTEXT-SEND-QUEUE-SIZE, ERR_CODE[%d]\n", error);
            return error;
        }

        /******************************************************************************
	**** On FI, Appl Task is used for RX IP pkts and NI has a special IP-Rx Task.
        **** So Assign ip.itc_ctx to appl_itc_context
        *******************************************************************************/
        appl_itc_context = context;
	ip.itc_ctx       = context;

	return ITC_OK;
}

ITC_ERROR appl_register_itc_callbacks(void)
{
	ITC_ERROR error;

	if ((error = itc_register_callbacks(appl_itc_context, appl_task_callback_registrations,
					    ARRAY_LEN(appl_task_callback_registrations))) != ITC_OK) 
	{
		uprintf("error - appl_task: itc_register_callbacks() failed\n");
		return error;
	}

	ip_register_itc_callbacks();

	#ifdef __IPV6__
	udp6_itc_init();
	#endif __IPV6__		
	
	icmp_itc_init();
	dns_itc_init();
#ifdef __IPV6__
	dns6_itc_init();
#endif __IPV6__

	return ITC_OK;
}

static void appl_task_init_post_phase()
{
	itc_msg_event *event_msg;
	UINT8 *proc = "appl_task_init_post_phase()";


        hal_appl_task_init_post_phase();
		system_app_init_phase3();

    /* Inform PLM module to start in STAT Mode and enable Mode Button */
    TRIGGER_LED_EVENT (PLM_MSG_STATUS_BUTTON_EVENT, PLM_STAT_MODE);

	// Send system init completion event:
	if ((event_msg = AllocEvent(EVENT_ID_INFRA_SYSTEM_INIT_COMPLETE, ( sizeof(itc_msg_event) + sizeof(msg_event_system_init_completed)))))
	{
	    msg_event_system_init_completed *msg = (msg_event_system_init_completed *)&event_msg[1];
		msg->bSuccess = 1;
		scp_send_event(ITC_APP_SCP, event_msg, 0, 0); // do not block and do not wait for ack
	}
	else
	{
		debug_uprintf("error - %s - alloc_event for EVENT_ID_INFRA_SYSTEM_INIT_COMPLETE failed\n", proc);
	}
}

extern INT32 csp_pe_attach_handler  (PORTEXT_EVENT_DATA *portext_event_data, PORTEXT_EVENT event);

static void appl_event_handler(void *req_ptr, void *resp_ptr)
{
	UINT16 i;
	PORT_ID port_id;
	itc_msg_event *req = (itc_msg_event *)req_ptr;
	msg_event_task_init_completed *task_init_comp_event_msg;
	PORTEXT_EVENT_DATA *portext_event_data = NULL;
	msg_event_event_complete *event_complete_data = NULL;

	UINT8 *proc = "appl_event_handler()";

	switch (req->event_id) 
	{
		case EVENT_ID_INFRA_TASK_INIT_COMPLETE:
			task_init_comp_event_msg = (msg_event_task_init_completed *) ((char *) req + sizeof(itc_msg_event));
			if (task_init_comp_event_msg->bSuccess)
			{
			  trace(INFRA_TRACE,
				TRACE_BOOTUP_DEBUG,
				"All-task initialization completed...final phase of system initialization started.\n");

			  appl_task_init_post_phase();

			}
			else
			{
			  kprintf("ERROR: All-task initialization failed...final phase of system initialization will not happen.\n");
			}
			break;

		case EVENT_ID_SPX_PE_ATTACH:			
		case EVENT_ID_SPX_PE_DETACH:
			portext_event_data = (PORTEXT_EVENT_DATA *) ((char *)req + sizeof (itc_msg_event));
			spx_scp_event_handler(portext_event_data, req->event_id);
			break;

		case EVENT_ID_EVENT_COMPLETE:
			event_complete_data = (msg_event_event_complete *) ((char *)req + sizeof (itc_msg_event));
			switch (event_complete_data->completed_event_id)
			{
				case EVENT_ID_SPX_PE_DETACH:
				{
						portext_event_data = (PORTEXT_EVENT_DATA *) &event_complete_data->event_data;
						spx_scp_event_handler(portext_event_data, req->event_id);
				}
				break;
			}
			break;
		case EVENT_ID_ISSU_VSYNC_DONE: {
			int *unit = (int *) ((char *)req + sizeof (itc_msg_event));
			kprintf("EVENT_ID_ISSU_VSYNC_DONE received for unit :%d\n", *unit);
			break;
		}
        case EVENT_ID_LICENSE_CHANGE: {
            kprintf("EVENT_ID_LICENSE_CHANGE received\n");
            myself_update_license();
            break;
        }

        case EVENT_ID_IP_ADDR_CHANGE_UPDATE_DHCPC: 
        {
			EVENT_ID_IP_ADDR_CHANGE_UPDATE_DHCPC_TYPE *data = (EVENT_ID_IP_ADDR_CHANGE_UPDATE_DHCPC_TYPE *) ((char *)req + sizeof (itc_msg_event));
			
			if(dhcpc_is_printf_group_debug_set())
    			kprintf("\nEVENT_ID_IP_ADDR_CHANGE_UPDATE_DHCPC received, port_number:%d, Action: %s \n", data->port_number, data->add_ip ? "ADDED" : "DELETED");
    			
			dhcpc_handle_ip_addr_change_event(data->port_number, data->add_ip);
		}
        break;

		default:
			kprintf("error - %s - unregistered event %d received\n", 
				proc, req->event_id);
			break;
	}
}

UINT32 appl_register_events()
{
	SCP_EVENT_ERROR err;

	/* Initialize Event */
	if ((err = RegisterEventCallback(appl_itc_context, appl_event_handler)) != SCP_EVENT_OK) {
		kprintf("Appl: Unable to initialize event system (error %d)", err);
		return FALSE;
	}

	err = RegisterEvent(appl_itc_context, EVENT_ID_INFRA_TASK_INIT_COMPLETE, SCP_EVENT_PRI_INFRA_HIGH);
	if (err != SCP_EVENT_OK)
	{
		kprintf("appl_register_events: error - RegisterEvent failed for event %d:%s with error %d\n", 
			EVENT_ID_INFRA_TASK_INIT_COMPLETE, "EVENT_ID_INFRA_TASK_INIT_COMPLETE", err);
		return FALSE;
	}
    
    err = RegisterEvent(appl_itc_context, EVENT_ID_LICENSE_CHANGE, SCP_EVENT_PRI_INFRA_HIGH);   //TBD_MINIONS
    if(err != SCP_EVENT_OK){
        kprintf("appl_register_events: error - RegisterEvent failed for event %d:%s with error %d\n",
            EVENT_ID_LICENSE_CHANGE, "EVENT_ID_LICENSE_CHANGE", err);
    }

#ifdef __PORT_EXTENSION__
	portext_scp_register_events();
#endif /*__PORT_EXTENSION__*/

	register_issu_sync_complete_event(appl_itc_context);

	register_dhcpc_ip_address_change_event(appl_itc_context);

	return TRUE;
}

void appl_main_timer_callback(void *req, void *res)
{
	ITC_MSG_TYPE msg_type = ITC_MSG_GET_MSG_TYPE(req);
        ITC_CONTEXT            context = itc_get_context_using_task_handle(sys_get_current_task_handle());
	ITC_APP_ID             app_id;
	ITC_ERROR              itc_stat = ITC_OK;
	itc_msg_timer_timeout *tmr_msg;
#ifdef FI_TIMER_LIB_IMPL
	UINT32 start_tick = sys_get_timeticks();
#endif


	if (msg_type != ITC_MSG_TYPE_TIMER_TIMEOUT)
		return;

	tmr_msg = (itc_msg_timer_timeout *)req;

	itc_stat = itc_get_app_id(context, &app_id);
	if (itc_stat != ITC_OK)
	{
	  trace(INFRA_TRACE,
		TRACE_CRITICAL,
		"appl_main_timer_callback: itc_get_app_id returned error status %d for the ITC context of the current task.\n",
		itc_stat);
	  return;
	}

	trace(INFRA_TRACE,
	      TRACE_DEBUG,
	      "appl_main_timer_callback: %s itc app is called back.\n",
	      itc_app_id_get_name(app_id));

#ifndef EVAL_BOARD
	if (sw_inf_fi_exec_timer_node_callback(tmr_msg->arg) == tmr_msg->arg)
	{
	  ITC_REQUEST_CALLBACK   itc_callback;

	  itc_callback = sw_inf_fi_get_itc_timer_event_callback();
	  if (itc_callback != NULL)
	  {
	    trace(INFRA_TRACE,
		  TRACE_INFO,
		  "appl_main_timer_callback: regular timer event callback is executed in app %s.\n",
		  itc_app_id_get_name(app_id));
	    (*itc_callback)(req, res);
#ifdef FI_TIMER_LIB_IMPL
	    timer_expiry_debug_print(start_tick, itc_callback, "Appl-Callback");
#endif
	  }
	  else
	    trace(INFRA_TRACE,
		  TRACE_ERROR,
		  "appl_main_timer_callback: timer arg 0x%x has no callback info in app %s.\n",
		  tmr_msg->arg, itc_app_id_get_name(app_id));
	}
#endif
}

static void handle_itc_uprintf_output_msg(void *request_ptr, void *response_ptr)
{
	//dummy handler which ignores uprintf messages
	//this is done so the reliable IPC mechanism and RConsole printfs work.
}

#ifdef SR_SWITCH_ROUTER

static void print_received_info(RTM_IPV4_CLIENT_REQ *req) {
    
    trace(L3_INTERFACE_TRACE_UTILITY,
          TRACE_DEBUG,     
          "\r\nSession ID: %d, \n"
          "DA:%m, SA:%m, Vlan ID:%d, \n"
          "Ether Type:0x%X, Outgoing Port %d:%p, \n"
          "NH IP:%I, SIP: %I, DIP:%I, VRF:%d:%s\n",
          "Non Reachability Reason: %d:%s\n",
          req->client_in_req.client_in.client_session_id,
          &req->client_in_req.da_mac,
          &req->client_in_req.sa_mac,
          req->client_in_req.vlan_id,
          req->client_in_req.client_ethertype,
          req->client_in_req.outgoing_port_number,
          req->client_in_req.outgoing_port_number,
          req->client_in_req.next_hop_addr,
          req->client_in_req.client_in.src_ip_address,
          req->client_in_req.client_in.dest_ip_address,
          req->client_in_req.client_in.vrf_idx,
          ipvrf_api_get_vrf_name(req->client_in_req.client_in.vrf_idx),
          req->client_in_req.client_in.non_reachability_reason,  
          get_rtm_ipv4_nonreachability_reason_str(req->client_in_req.client_in.non_reachability_reason));
}

/*
 * ITC Notification sequence from L3/RTM:
 * 1. After registration with RTM, first notification (RTM_CLIENT_ROUTE_AVAIL) will come from RTM
 *    when ever route will be available for registered destination ip.
 *    If no notification coming means, Route is not available.
 * 2. If Route was (RTM_CLIENT_ROUTE_AVAIL) available (Notification has been send in step 1) .
 *    Notification (RTM_CLIENT_ROUTE_NOT_AVAIL) will be sent, if route/reachability becomes deleted/not exist for destination IP.
 *    Notification (RTM_CLIENT_ROUTE_UPDATE) will be sent, if route/reachability becomes changed for destination IP.
 * 3. If route becomes available again for Destination IP then notification (RTM_CLIENT_ROUTE_AVAIL) will be given.
 */
static void appl_itc_rtm_route_not_avail (void *request_ptr, void *response_ptr)
{
    RTM_IPV4_CLIENT_REQ *req = (RTM_IPV4_CLIENT_REQ *)request_ptr;
    
    trace(L3_INTERFACE_TRACE_UTILITY,
          TRACE_DEBUG,
          "\r\nReceived RTM_CLIENT_ROUTE_NOT_AVAIL Msg");

    if(req) {
        print_received_info(req);
        /*
         * Perform Required Action
         */
        /* ERSPAN */
        event_erspan_route_status(RTM_CLIENT_ROUTE_NOT_AVAIL, req);

    } else {
        uprintf("\r\nError: Invalid Input Req");
    }
}

static void appl_itc_rtm_route_avail (void *request_ptr, void *response_ptr)
{
    RTM_IPV4_CLIENT_REQ *req = (RTM_IPV4_CLIENT_REQ *)request_ptr;
 
    trace(L3_INTERFACE_TRACE_UTILITY,
          TRACE_DEBUG,    
          "\r\nReceived RTM_ROUTE_AVAIL Msg");

    if(req) {
        print_received_info(req);
        /*
         * Perform Required Action 
         */
       /* ERSPAN */
       event_erspan_route_status(RTM_CLIENT_ROUTE_AVAIL, req);
    } else {
        uprintf("\r\nError: Invalid Input Req");
    }
}   

static void appl_itc_rtm_route_update (void *request_ptr, void *response_ptr)
{
    RTM_IPV4_CLIENT_REQ *req = (RTM_IPV4_CLIENT_REQ *)request_ptr;
 
    trace(L3_INTERFACE_TRACE_UTILITY,
          TRACE_DEBUG,    
          "\r\nReceived RTM_ROUTE_UPDATE Msg");

    if(req) {
        print_received_info(req);
        /*
         * Perform Required Action 
         */
        /* ERSPAN */
        event_erspan_route_status(RTM_CLIENT_ROUTE_UPDATE, req);

    } else {
        uprintf("\r\nError: Invalid Input Req");
    }
}   
#endif SR_SWITCH_ROUTER

/***************************************************************************
 * Name: appl_itc_vlan_entry_update_callback
 *
 * Description: application callback function for vlan_entry update ITC msg
 *
 * Parameters: req_ptr and resp_ptr 
 *
 * Return: N/A
 *
 * Method:
 *
 ***************************************************************************/
void appl_itc_vlan_entry_update_callback(void *req_ptr, void *resp_ptr)
{
    VLAN_ENTRY_UPDATE_REQ_T *req = (VLAN_ENTRY_UPDATE_REQ_T *)req_ptr;
    INT32 ret;

    if(req)
    {
       ret = cu_set_n_sync_vlan_port_mask(req->vlan_id, req->port_mask, req->oper, req->tag_type, 0, MAX_TOTAL_SESSIONS);
       if(ret != CU_OK)
       {
          fitrace(VLAN_TRACE, FITRACE_VLAN_SYNC, TRACE_ERROR, "appl_itc_vlan_entry_update_callback: cu_set_vlan_port_mask failed for vlan:%d\n", req->vlan_id);
       }
    }
    else
    {
       fitrace(VLAN_TRACE, FITRACE_VLAN_SYNC, TRACE_ERROR, "appl_itc_vlan_entry_update_callback: Error - Invalid Inut Req\n");
    }
}

/***************************************************************************
 * Name: appl_send_vlan_update_evt_req
 *
 * Description: send vlan event request to application task
 *
 * Parameters: evt_req
 *
 * Return: CU_OK/CU_ERROR
 *
 * Method:
 *
 ***************************************************************************/
int appl_send_vlan_update_evt_req(VLAN_ENTRY_UPDATE_REQ_T *evt_req)
{
    ITC_ERROR itc_error;

    itc_error =  itc_send_request(itc_get_context_using_task_handle(sys_get_current_task_handle()), ITC_APP_APPL,
                                    ITC_MSG_TYPE_VLAN_ENTRY_UPDATE, evt_req, sizeof(VLAN_ENTRY_UPDATE_REQ_T), ITC_PRI_HIGH, 0, 0);

    if ( itc_error != ITC_OK )
    {
        return CU_ERROR;
    }
    return CU_OK;
}

#define INIT_STEP_START                 0x00
#define INIT_STEP_ZERO_DONE             0x01
#define INIT_STEP_NO_WAIT_START         0x0F
#define INIT_STEP_FREE_RUN_START        0xFF

static UINT8 task_done = 0;
static UINT8 initStepStage = INIT_STEP_START;

/*
 * Informs the main task about the completion of task initialization and
 * awaits to be notified by the main task for proceeding with the next
 * stage initialization/configuration, etc
 */
static void task_continue_init(unsigned int tid);
void task_init_complete(TASK_PARAM *task_param, int wait)
{
	int count = fi_Task_Get_Total_Count();

	task_done ++;

	if ( task_done >= count ) {
/*459863 : this message gets printed 3 times during TASK initialization for each stage which is misleading. So commenting out this* 
  *Anyways we could see the Task init Completion by the message "Staged Init Done. Global Lock Synchronization Start."*/
//	 	uprintf("TASK Init Step Completed\n\n");

		task_done = 0;

		/* send status to main task */
		if (SYS_OK != sys_give_semaphore(task_param->mtask_sem, 1))
		{
			kprintf("ERROR: Can't send event to main task!\n");
		}
	}
	else if ( initStepStage == INIT_STEP_START ) {
		/* During Task creating each task sends the ready
		   signal directly to the main task.  */
		if (SYS_OK != sys_give_semaphore(task_param->mtask_sem, 1))
		{
			kprintf("ERROR: Can't send event to main task!\n");
		}		
	}
	else {
		task_continue_init(task_done);
	}

	/* Returning without blocking for Staged initialization is 
	   allowed only for the last stage of initialization. */
	if ((!wait) && (initStepStage >= INIT_STEP_NO_WAIT_START))
		return;

	/* wait for main task to notify to continue with init */
	if (SYS_OK != sys_take_semaphore(task_param->stask_sem, 1, SYS_FOREVER))
	{
		kprintf("ERROR: Can't receive event from main task!\n");
	}
}

/*
 * Main task is waiting for the task initialization of all the tasks
 * to complete. When the task completes it's initialization, it sends
 * an event with sub-task unique event.
 */
static void await_task_init(int mtask_sem)
{
	int count = fi_Task_Get_Total_Count();

	if (SYS_OK != sys_take_semaphore(mtask_sem, 1, SYS_FOREVER))
	{
		kprintf("ERROR: Can't receive event from sub task!\n");
	}
}

/*
 * Main task informs the sub tasks to continue with the initialization
 * and/or configuration, etc. Basically transitioning from one stage to
 * next stage.
 */
static void task_continue_init(unsigned int tid)
{
    int i     = fi_Task_Get_Total_Count();
    int count = fi_Task_Get_Total_Count();

    kick_watchdog();
    
	if (tid >= i)  {
		uprintf("%s>ERROR %d\n", tid);
		return;
	}

	if (initStepStage != INIT_STEP_FREE_RUN_START)
        {
		if (SYS_OK != sys_give_semaphore(appl_tasks[tid].task_param.stask_sem, 1))
			kprintf("ERROR: Can't send event to sub task!\n");
	}
	else
        {
		/* All init done, let us start all the tasks now.  */
		for (i = 0; i < count; i++)
		{
			if (SYS_OK != sys_give_semaphore(appl_tasks[i].task_param.stask_sem, 1))
			    kprintf("ERROR: Can't send event to sub task!\n");
		}
	}
}

// This function should be called before creating app tasks and 
// purpose is to load the sys max values parsed from config to global variables, 
//  which are used by app tasks in stage 1/stage2 initialization

void sw_load_sys_max_limits_to_globals(void)
{
#ifdef SR_SWITCH_ROUTER
    if (hal_ACL_Is_LocalNode_PE_Capable())
    {
	INT32 max_router_int = cu_get_max_param(MAX_ROUTER_INT_INDEX);

	cu_set_config_param(MAX_ROUTER_INT_INDEX, max_router_int);
	cu_set_curr_param(MAX_ROUTER_INT_INDEX, max_router_int);
    }

  g_sw_sys.max_router_int = cu_get_curr_param(MAX_ROUTER_INT_INDEX);
#endif SR_SWITCH_ROUTER
  // FINI TODO initialize any other glabal variables here if needed.
}

// This function returns TRUE if the specified task handle belongs to appl task.
// NOTE: Currently, the check uses hardcoded value of 1 as the index into the
//       appl_tasks array for the fastest check as optimization.
enum BOOLEAN is_task_handle_for_appl_task(int taskHandle)
{
  return (appl_tasks[1].sys_task_info == taskHandle);
}

void create_appl_tasks(void)
{
	int            count = fi_Task_Get_Total_Count();
	int            mtask_sem, stask_sem, i;
	char           sub_task_event_name[SYS_MAX_NAME_LENGTH+1];  
	itc_msg_event *event_msg;
	UINT8         *proc = "create_appl_tasks()";

	// System is initializing, let us queue the syslog and trap packets.
	g_sw_sys.links_up_state = FALSE;
	// Init shutdown flag so it can be used later during boots
	g_sw_sys.shutdown_in_progress = FALSE;
	
	syslog_snmp_udp_packet_queue_size = 0;
    
    // We need to keep the fi_appl_lock initialization here to prevent a race
    // condition that has been seen during bringup
    // Initialize fi_appl_lock
    sw_inf_init_fi_appl_lock();

    // Keeping this lock creation in the timer task context for now
//    sw_inf_fi_init_sv_timer_lock();
	/*
	* Start Keygen task for key generation
	*/
	g_task_keygen = sys_create_task(KEYGEN_TASK_NAME, keygen_task, 0, KEYGEN_TASK_PRIORITY,
							  DEFAULT_TASK_STACK_SIZE, TEMPORARY_COMMON_VSID,
							  SYS_PREEMPTIVE);
	if(g_task_keygen==0)	
	{
		kprintf("\nFATAL ERROR: can't create keygen task\n");
		trace(SXR_RED,TRACE_CRITICAL,"FATAL ERROR: can't create keygen task\n");
		sys_exit(1);
	} 


	/*
	* Start ITC task & define ITC messages
	*/
	if (0 == sys_create_task(ITC_TASK_NAME, itc_task_main, 0, ITC_TASK_PRIORITY,
							  ITC_TASK_STACK_SIZE, TEMPORARY_COMMON_VSID,
							  SYS_PREEMPTIVE))
	{
		kprintf("\nFATAL ERROR: can't create ITC task\n");
		trace(SXR_RED,TRACE_CRITICAL,"FATAL ERROR: can't create ITC task\n");
		sys_exit(1);
	} 


	sys_yield();

	if (fi_Task_All_Define_ITC_Messages() != 0)
	{
            kprintf("\nFATAL_ERROR: Can't Define ITC Messages\n");
	    sys_exit(1);
	}

	if (g_sptr_syslog != NULL)
	{
		g_sptr_syslog->enabled = 0;
	}

	system_hw_init();

	#ifndef NO_OPTICAL_MONITOR
		extern void tanto_om_scheduler_init();
		tanto_om_scheduler_init();

	#endif NO_OPTICAL_MONITOR


#ifdef FI_TIMER_LIB_IMPL
	/* Initialize the Timer Library*/
	timer_lib_init();
#else
	/*
	 * Start timer && any other tasks not in appl_tasks[]
	 */
	g_timer_task = sys_create_task(TIMER_TASK_NAME, (int (*)(unsigned int))timer_task, 0, TIMER_TASK_PRIORITY,
				       DEFAULT_TASK_STACK_SIZE, TEMPORARY_COMMON_VSID, SYS_PREEMPTIVE);
	if (!g_timer_task) {
		kprintf("FATAL ERROR: can't create timer task\n");
		sys_exit(1);
	}
#endif

	if (fips_cryptoInit() != CU_OK)
	{
		kprintf("Crypto Module failed to initialize \n");
		sys_exit(1);
	}
	cmd_init();

	// initialize the max limits, as per sys-mac configuration
	sw_load_sys_max_limits_to_globals();

	/*
	 * Create the main task and sub-tasks synchronization events for the
	 * main-sub tasks synchronization during task initialization.
	 */
	if (0 == (mtask_sem = sys_create_semaphore(MAIN_TASK_EVENT, 0)))
	{
		kprintf("\nCan't create main task event, exiting!\n");
		sys_exit(1);
	}

	for (i = 0; i < count; i++)
	{
		/* create a unique name for the sub task event */
		ksprintf(sub_task_event_name, "%s_%d", SUB_TASK_EVENT, i);

		/* create an event for sub-task synchronization with the main task */
		if (0 == (stask_sem = sys_create_semaphore(sub_task_event_name, 0)))
		{
			kprintf("\nCan't create sub task event, exiting!\n");
			sys_exit(1);
		}

		/* setup the task parameter */
		appl_tasks[i].task_param.mtask_sem = mtask_sem;
		appl_tasks[i].task_param.stask_sem = stask_sem;
		appl_tasks[i].task_param.stask_id = i;
	}

	for (i = 0; i < count; i++)
	{
		if (appl_tasks[i].sys_task_info)  // task already created
			continue;

#ifdef FI_SMP_SUPPORT
#if 0
if (!strcmp(appl_tasks[i].task_name,IKE_TASK_NAME))
{

		appl_tasks[i].sys_task_info = sw_inf_fi_appl_create_task(
										appl_tasks[i].task_name,
										(int (*)(unsigned int))
										appl_tasks[i].task_entry,
										(unsigned)&appl_tasks[i].task_param,
										appl_tasks[i].priority,
										appl_tasks[i].stack_size,
										appl_tasks[i].vsid,
										(SYS_PREEMPTIVE | (1 << 1))); // COnsidering only 2 core now . It has to be made generic

}else
#endif
 if ((!strcmp(appl_tasks[i].task_name,IKE_MAIN_TASK_NAME))
#ifdef __PKI__
     || (!strcmp(appl_tasks[i].task_name,PKI_TASK_NAME))
#endif
#ifdef __HTTPC__
    || (!strcmp(appl_tasks[i].task_name,HTTP_CLIENT_TASK_NAME))
#endif
    )
{

		appl_tasks[i].sys_task_info = sw_inf_fi_appl_create_task(
										appl_tasks[i].task_name,
										(int (*)(unsigned int))
										appl_tasks[i].task_entry,
										(unsigned)&appl_tasks[i].task_param,
										appl_tasks[i].priority,
										appl_tasks[i].stack_size,
										appl_tasks[i].vsid,
										(SYS_PREEMPTIVE | (1 << 1))); // COnsidering only 2 core now . It has to be made generic
}else

#endif

		appl_tasks[i].sys_task_info = sw_inf_fi_appl_create_task(
										appl_tasks[i].task_name,
										(int (*)(unsigned int))
										appl_tasks[i].task_entry,
										(unsigned)&appl_tasks[i].task_param,
										appl_tasks[i].priority,
										appl_tasks[i].stack_size,
										appl_tasks[i].vsid,
										SYS_PREEMPTIVE);
	
		if (!appl_tasks[i].sys_task_info)
		{
			trace(SXR_RED, TRACE_CRITICAL, "\nFATAL ERROR: can't create %s task, abort!! \n", appl_tasks[i].task_name);
			sys_exit(1);
		}
		else
		{
		    //  Set task application ID to keep track of BM buffer ownership
	    	sys_set_task_appid(appl_tasks[i].task_name, appl_tasks[i].task_app_id);
		}

		// Wait for the task creation to occur and  task will wait at the 
		// entry point for synchronized initialization.
		await_task_init(mtask_sem);
  	}

	/* At this stage all tasks has been created and are waiting just
	   after the entry point function execution for the signal from 
	   the main task. From this point to the stage where initStepStage
	   is set to INIT_STEP_NO_WAIT_START, each stage of initialization
	   is controlled. The initialization occurs as per the task
	   sequence definition in appl_tasks. */
	initStepStage = INIT_STEP_ZERO_DONE;
	task_continue_init(0);

	/* Wait for all the tasks to complete 1st stage initialization */
	await_task_init(mtask_sem);

	/*
	 * 2nd stage task initialization :
	 *
	 * As all the tasks have successfully completed the initialization,
	 * notify the tasks to proceed with second stage initialization,
	 * which is attaching resources, registration, etc.
	 */
	task_continue_init(0);

	/* Wait for all the tasks to complete 2nd statge initialization */
	await_task_init(mtask_sem);

	/* From the next stage of initialization, the task's have an
	   option to not wait for the signal from the creation task. The
	   init step flag is moved to a different value to enable this
	   option. */
	initStepStage = INIT_STEP_NO_WAIT_START;

	/*
	 * 3rd stage task initialization :
	 *
	 * As all the tasks have successfully completed the initialization,
	 * notify the tasks to proceed with third stage initialization,
	 * which is setting up the configuration from the config file.
	 */
	task_continue_init(0);

	/* Wait for all the tasks to complete all their initializations */
	await_task_init(mtask_sem);

	/* Last stage of staged initialization has reached. The next time
	   task_continue_init() is called all the waiting tasks will be
	   released simultaneously. */
	initStepStage = INIT_STEP_FREE_RUN_START;
	
	//kprintf("All L3 tasks have completed their initializations\n");


	/* Initialize the Task CPU Usage History Trace */
	sil_init_ts_db(FALSE);

#ifndef OFFICIAL_RELEASE
	uprintf("\nStaged Init Done. Global Lock Synchronization Start.\n");
#endif
	task_continue_init(0);

	//FINIL3 start
	/*
	  * initialize the IPC library. Unlike SX platform, IPC library in CH is not initialized during
	  * system initialization. It is done on demand when stacking is enabled. For NI L3 code, IPC
	  * library is ALWAYS required to make mcast task communicate with application task in same unit.
	  */ 
	ipc_init();
	//FINIL3 end

	g_all_task_initialization_done = 1;

	sw_inf_take_fi_appl_lock();
	// Send all task init completion event:
	if ((event_msg = AllocEvent(EVENT_ID_INFRA_TASK_INIT_COMPLETE, (sizeof(itc_msg_event) + sizeof(msg_event_task_init_completed)))))
	{
	    msg_event_task_init_completed *msg = (msg_event_task_init_completed *)&event_msg[1];
		msg->bSuccess = 1;
		scp_send_event(ITC_APP_SCP, event_msg, 1, 0); // block for event completion by all, but no ack
	}
	else
	{
		debug_uprintf("error - %s - alloc_event for EVENT_ID_INFRA_TASK_INIT_COMPLETE failed\n", proc);
	}
	sw_inf_give_fi_appl_lock();
}

#ifdef __IP_MULTICAST__
#if 0 //FINIL3
/* these functions take care of msg to Appln task for mcast module */
void l2mcastlp_ipc_itc_msg_callback(void *req, void *res)
{
	ipc_message_struct *ipc_msg = (ipc_message_struct *)((UINT8 *)req + sizeof(ITC_MSG_HEADER));
	void *msg_ptr = (UINT8 *)req + sizeof(ITC_MSG_HEADER) + sizeof(ipc_message_struct);
	l2mcastlp_process_ipc (ipc_msg->data_size, msg_ptr, 0, 0);
}
#endif

#ifdef SR_SWITCH_ROUTER
void mcastlp_ipc_itc_msg_callback(void *req, void *res)
{
	ipc_message_struct *ipc_msg = (ipc_message_struct *)((UINT8 *)req + sizeof(ITC_MSG_HEADER));
	void *msg_ptr = (UINT8 *)req + sizeof(ITC_MSG_HEADER) + sizeof(ipc_message_struct);
	mcastlp_process_ipc (ipc_msg->data_size,  msg_ptr, 0, 0);
}

void mcast6lp_ipc_itc_msg_callback(void *req, void *res)
{
	ipc_message_struct *ipc_msg = (ipc_message_struct *)((UINT8 *)req + sizeof(ITC_MSG_HEADER));
	void *msg_ptr = (UINT8 *)req + sizeof(ITC_MSG_HEADER) + sizeof(ipc_message_struct);
	mcastlp6_process_ipc (ipc_msg->data_size,  msg_ptr, 0, 0);
}

#endif SR_SWITCH_ROUTER

#endif

#ifdef FI_CPU_SAMPLE_DEBUG

void register_all_fi_tasks_for_sampling()
{
	int count = fi_Task_Get_Total_Count();
	int i;

	for (i = 0; i < count; i++) {
		sw_inf_fi_start_sample_for_task(appl_tasks[i].task_name);
	}
}

void get_stack_trace_all_fi_tasks()
{
	int count = fi_Task_Get_Total_Count();
	int i;

	for (i = 0; i < count; i++)
        {
		sw_inf_fi_print_other_task_sample(appl_tasks[i].task_name);
	}
}
#endif // FI_CPU_SAMPLE_DEBUG

void sys_reset_appl_cleanup()
{
	extern int gridiron_initialization_done;

	/* Cleanup functions in application in case the switch is
	   restarted. 
	   NOTE: This function doesn't get executed when the system
	   has crashed.
	*/

	/* Poll the Tx queues in case a packet has been sceduled for
	   transmission. This is done only once the switch
	   initialization is completed. */
	if (gridiron_initialization_done != 0) 
		pp_service_transmit_descriptor_queues();

	/* Cleanup the console output buffer. */
	console_reset_cleanup();
}
