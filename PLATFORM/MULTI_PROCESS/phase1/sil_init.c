/*
 * Copyright (C) 2003, 2004, 2005 Foundry Networks
 * Copyright (C) 2010 Brocade Communications Systems
 *
 * Unpublished - rights reserved under the Copyright Laws of the United States.
 * Use, duplication, or disclosure by the Government is subject to restrictions
 * as set forth in subparagraph (c)(1)(ii) of the Rights in Technical Data and
 * Computer Software    clause at 252.227-7013.
 */
/*
 *
 * Filename :
 *      sil_init.c
 * Description :
 *      This file handles SIL initialization
 * Initiation Date :
 *      17-June-2011
 * Initiation Release :
 *      FI 7.4
 * Releases Applicable :
 *      FI 7.4, <keep adding new releases>
 * Platforms Applicable :
 *      KX, FX, SX Family of platforms
 * Modification Guidelines :
 *      - Please add code that are specific to SIL initialization
 *      - Any checkin to this file need code review and approval from
 *        platform group members
 * Current State :
 *      Active
 * Author :
 *      Rajkumar Sivasamy
 * Owner(s) :
 *      Katara Platform Group
 * Copyright :
 *      Brocade Communication Systems, Inc. (C) 2010+
 *
 */
#define _GNU_SOURCE
#include "sil.h"
#include "sil_external.h"
#include <sched.h>

#define MAIN_THREAD
/* Flag which indicates whether SIL init is done or not */
int sil_init_done;
/* Board Id */
int silBoardId = -1;
handle_t g_pause_fiapp_lock = -1;
extern void sil_check_remote_debugging(void);
#ifdef STACK_MEM_DUMP
/* FD for the stack dump file */
int g_stk_dump_fd = -1;
/* Flag to decide if we need to print stack contents or not */
int g_stack_print = FALSE;
#endif
extern void set_cut_through_enable(int enable);
int g_auto_upgrade_uboot = 1;
int g_enable_fpga = 1;
int g_enable_pkttest = 0;
int g_enable_pkttest_log = 0;
int g_warm_reboot_ipsec_card = 0;
int g_fw_auto_upgrade = 1;
int g_appl_task_profiling = 0;
extra_bootargs_t extrabootargs = { 0 };

static void check_extra_bootargs(void)
{
	FILE *file;
	char str[PROC_CMDLINE_LEN];
	file = fopen(PROC_CMDLINE_PATH, "r");
	if (file == NULL) {
		SIL_LOG(CRIT, GENERIC, "!!! Unable to open the cmdline \n");
		return;
	}
	fgets(str, PROC_CMDLINE_LEN, file);
	memset(&extrabootargs, 0, sizeof(extra_bootargs_t));
	if (strstr(str, "nopolicer"))
		extrabootargs.nopolicer = 1;
	if (strstr(str, "debugoncrash"))
		extrabootargs.debugoncrash = 1;
	if (strstr(str, "nocoredump"))
		extrabootargs.nocoredump = 1;
	if (strstr(str, "nofiapp"))
		extrabootargs.nofiapp = 1;
#ifdef TODO
	/* pcl_id_mgr_add_pcl_id line 425 pcl_table points to 
	 * debug footer 0xFBFBFBFB, which leads to sigsegv 
	 */
#ifndef OFFICIAL_RELEASE
	extrabootargs.memdebug = 1;
#endif
#endif
	if (strstr(str, "nomallocdebug"))
		extrabootargs.memdebug = 0;
	else if (strstr(str, "mallocdebug"))
		extrabootargs.memdebug = 1;
	if (strstr(str, "mgmtdebug"))
		extrabootargs.mgmtdebug = 1;
	if (strstr(str, "mgmtpromisc"))
		extrabootargs.mgmtpromisc = 1;
	if (strstr(str, "remotedebug"))
		extrabootargs.remotedebug = 1;
	if (strstr(str, "noautostart"))
		extrabootargs.noautostart = 1;
	if (strstr(str, "nomod"))
		extrabootargs.nomod = 1;
	if (strstr(str, "nosoftwatchdog"))
		extrabootargs.nosoftwatchdog = 1;
	if (strstr(str, "enabletelnet"))
		extrabootargs.enabletelnet = 1;
	if (strstr(str, "skiperror"))
		extrabootargs.skiperror = 1;
	if (strstr(str, "storeforward")) {
		extrabootargs.storeforward = 1;
	}
	if (strstr(str, "sildebug"))
		extrabootargs.sildebug = 1;
	if (strstr(str, "disablefpga")) {
		g_enable_fpga = 0;
	}
	if (strstr(str, "disableautouboot")) {
		g_auto_upgrade_uboot = 0;
	}
	if (strstr(str, "disable-pkttest")) {
		g_enable_pkttest = 0;
	}
	if (strstr(str, "enable-pkttest")) {
		/* when packet test is enabled , default behavior is to warm-reboot
		 * of the service-module. */
		g_enable_pkttest = 1;
		g_warm_reboot_ipsec_card = 1;
	}
	if (strstr(str, "en-pkttest-log")) {
		g_enable_pkttest_log = 1;
	}
	if (strstr(str, "enable-tnls-reboot")) {
		g_warm_reboot_ipsec_card = 1;
	}
	if (strstr(str, "no_fwauto_upd")) {
		g_fw_auto_upgrade = 0;
	}
	fclose(file);
	return;
}

/*
 * sil_init
 *
 * Do following initialization:
 *   1) Open SIM Module
 *   2) Intercept Core generating signals
 *   3) MMAP DMA region to user space
 *   4) MMAP of Timer register so that user space can use high pression timer
 *   5) Initialization required to stitch TAP interface with SIL which is 
 *      required for OOB management porti
 *   6) Get the Image Partition Size based on the platform.
 */
static int sil_init(void)
{
	int ret, i;
	/* Initialize steps required for management interface */
	ret = sil_mgmt_port_init();
	if (ret < 0) {
		SIL_LOG(CRIT, GENERIC,
			"sil_init: Failed to do init for mgmt port, error: %d\n",
			ret);
		return -1;
	}
	/* Get the Flash Image Partition Size */
	ret = sil_get_image_partition_size();
	if (ret < 0) {
		SIL_LOG(CRIT, GENERIC,
			"sil_init: Failed to get the ImagePartition Size, error %d\n",
			ret);
		return -1;
	}
	/* Will open the Flash Partitions and set nicknames if needed */
	ret = sil_image_info_init();
	if (ret < 0) {
		SIL_LOG(CRIT, GENERIC,
			"sil_init: Failed to do image_info_init, error: %d\n",
			ret);
		return -1;
	}
	/* Populate the board ID and HW version */
	silBoardId = sys_get_board_id();
	if (silBoardId == -1) {
		SIL_LOG(CRIT, GENERIC,
			"sil_init: Failed to get the board id\n");
		return -1;
	}
	SIL_LOG(CRIT, GENERIC, "platform type %d\n", silBoardId);
	/* Set the save areas for crash dump info */
	sil_dm_save_init(silBoardId);
	ret = pbuf_init();
	if (ret != 0) {
		SIL_LOG(CRIT, GENERIC,
			"sil_init: Failed to initialize pbuff, error: %d\n",
			ret);
		return ret;
	}
	/* Check if FI-App need not to be started, so that init will stop at the
	 * OS Prompt only. This way user can do some debugging without starting FI App.
	 */
	if (extrabootargs.nofiapp) {
		SIL_LOG(CRIT, GENERIC,
			"WARNING!!! FastIron Application won't be started \n");
		tty_select_id(TTY_OWNER_OS);
		/* Disable Watchdog since there is no FI to kick Watchdog counter */
		sil_disable_watchdog();
	}
#ifdef SYMBOL_DEBUG
	/* Open symbol file and count number of symbols */
	count_no_of_symbols();
#endif
	/* See if we have hooked up GDB, if so, set a flag */
	sil_check_remote_debugging();
	g_pause_fiapp_lock = sys_create_semaphore("FI-App-Pause-Lock", 0);
	return 0;
}

/*
 * Main function of Application
 */
int main(int argc, char **argv)
{
	TASK *osTask, *tmrTask, *sigTask, *flashTask;
	TASK *fiTask;
	int retVal;
	TASK *mgmtPollTask, *intrTask;
	TASK *htbtTask;
	int max_cpus;
	max_cpus = sys_get_online_cpus();
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(0, &set);
	if (sched_setaffinity(0, sizeof(set), &set) == -1)
		SIL_LOG(CRIT, GENERIC, "./FastIron affinity can not be set \n");
	/* Flag to indicate basic SIL initialization done. */
	sil_init_done = FALSE;
	/* Create handler for shell commands which needs to be invoked using
	 * system() API. This needs to be done before creation of any threads
	 */
	extern handle_t syscmd_handle_lock;
#ifdef STACK_MEM_DUMP
	/* Create a file that can store the stack of the dying thread */
	g_stk_dump_fd = creat(STK_DUMP_PATH, S_IRUSR | S_IWUSR);
#endif
	/* Do task and synchronization init */
	sil_init_task();
	sil_init_sync();
	/* Do signal handling related initialization */
	sil_signal_handler_init();
	/* 
	 * Create Signal handler thread as early as possible since WD sends SIGUSR1 to sig-handle task ONLY.     
	 */

	sil_create_syscmd_handler();
	sigTask =
	    (TASK *) sys_create_task(SIG_HDLR_TASK_NAME, sil_signal_handler, 0,
				     SIG_HDLR_TASK_PRI, 16 * 1024, 0, 0);
	/* Parse the kernel commandline for extra_bootargs */
	check_extra_bootargs();
	if (extrabootargs.sildebug) {
		sil_debug_flag = 1;
		sil_debug_lvl = 1;
	}
#ifndef OFFICIAL_RELEASE
	/* Check if coredump collection is disabled via "extra_bootargs"
	 * Also check if remote debug session needs to be started after FI crash
	 */
	if (extrabootargs.nocoredump) {
		prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
		SIL_LOG(CRIT, GENERIC,
			"WARNING!!! === Core dump will not be saved on flash ===\n");
	}
	if (extrabootargs.debugoncrash) {
		SIL_LOG(CRIT, GENERIC,
			"WARNING!!! === Remote debugging session will be started "
			"after FastIron crash ===\n");
	}
#endif /* ifndef OFFICIAL_RELEASE */
//#ifdef FI_SMP_SUPPORT   
	retVal = sil_init();
	if (retVal != 0) {
		SIL_LOG(CRIT, GENERIC,
			"CRITICAL: Failed to do SIL Init, error: %d\n", retVal);
		/* Penalize the system by calling the task switch function, so that
		 * the watchdog counter gets incremented immediately
		 */
		while (1) {
			sys_sleep(10);
		};
	}
	/* Starting FIrmware IPC Infra */
	sil_fw_download_prologue(NULL);
	sil_platform_fw_check_and_download(g_fw_auto_upgrade);
	/* Create OS thread lock on which OS thread will be sleeping */
	//sil_os_thread_lock = sys_create_semaphore(SIL_OS_THREAD_LOCK, 0);
	/* Create OS thread with stack-size = 16kb */
	extern int os_thread_entry(unsigned int param);

	osTask = (TASK *) sys_create_task(OS_TASK_NAME, os_thread_entry, 0,
					  OS_TASK_PRI, 16 * 1024, 0, 0);
	/* Create Timer thread with stack-size = 16kb */
	tmrTask = (TASK *) sys_create_task(TIMER_TASK_NAME, sil_create_ptmr, 0,
					   TIMER_TASK_PRI, 16 * 1024, 0, 0);
	/* Create Flash thread with stack-size = 64kb */
	/* First create flash task; so that once main is started, we should be ready
	 * for any flash operation
	 */
	flashTask = (TASK *) sys_create_task(FLASH_TASK_NAME, flash_ftask, 0,
					     FLASH_TASK_PRI, 64 * 1024, 0, 0);

	/* Set sil_init_done flag
	 * IMP Note - Set this flag just before creating MainTsk, since uprintf()
	 * is available only after MainTsk comes into picture. Till that time LOG()
	 * should call printf() and it's decided by value of sil_init_done
	 */
	tty_set_raw();
#ifdef CUT_THROUGH
	if ((extrabootargs.storeforward) == TRUE) {
		set_cut_through_enable(0);
	}
#endif
	if ((extrabootargs.nofiapp) == TRUE) {
		/* Now block on g_pause_fiapp_lock */
		sys_take_semaphore(g_pause_fiapp_lock, 1, SYS_FOREVER);
		/* Enable SW watchdog */
		sil_enable_watchdog();
		sil_init_done = TRUE;
		/* Make FI as console owner */
		tty_select_id(TTY_OWNER_FI);
		tty_unget_c('\r');
	} else {
		/*
		 * IMP Note - Do this only after sil_init_done is set means we can start
		 * using uprintf() instead of printf().
		 */
		sil_init_done = TRUE;
	}
	sys_kernel_syncup_to_rtc();
	syscmd_handle_lock = sys_create_semaphore("SYSCMD_HANDLER_LOCK", -1);
#ifdef MAIN_THREAD
	/* Create FI-Main thread with stack-size = 1mb */
	fiTask = (TASK *) sys_create_task(FI_TASK_NAME, fi_main, 0,
					  FI_TASK_PRI, 4 * 1024 * 1024, 0, 0);
#else
	tty_select_id(TTY_OWNER_OS);
#endif
	/* Create Mport-poll thread with stack-size = 16kb */
	mgmtPollTask =
	    (TASK *) sys_create_task(MPORT_TASK_NAME, sil_mport_select, tapFd,
				     MPORT_TASK_PRI, 16 * 1024, 0, 0);
#ifdef AUX_THREADS
	/* Create Interrupt thread with stack-size = 16kb */
	intrTask = (TASK *) sys_create_task(INTER_TASK_NAME, sil_intr_task, 0,
					    INTER_TASK_PRI, 16 * 1024, 0, 0);
#endif

	/*Indra create FI thread here as well to check */
	/* Wait on OS thread */
	pthread_join(osTask->tid, NULL);
	//extern int task_main(int argc, char **argv);
	//task_main(0, NULL);
	/* This should never reach */
	return 0;
}
