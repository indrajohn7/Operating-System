/************************************************************************/
/*      Copyright (C) 1996-1997 Foundry Networks                                                        */
/*      Unpublished - rights reserved under the Copyright Laws of the           */
/*      United States.  Use, duplication, or disclosure by the                          */
/*      Government is subject to restrictions as set forth in                           */
/*      subparagraph (c)(1)(ii) of the Rights in Technical Data and             */
/*      Computer Software clause at 252.227-7013.                                                       */
/* SwAging.c                                                            */
/*      Switching aging definations .                                                       */
/************************************************************************/
#include <sys/socket.h>
#include <netinet/in.h>
#include "sw.h"
#include "ip.h"
#include "ipfilter.h"
#include "vipport.h"
#include "snmptrap.h"

#ifdef SR_SWITCH_ROUTER
#ifndef NO_USE_L4_CAM_FOR_PRIORITY
#include "aghlst.h"
#include "vipaccst.h"
#endif SR_SWITCH_ROUTER
#endif NO_USE_L4_CAM_FOR_PRIORITY

#ifdef MPLS
#include "vmplsst.h"
#include "mpls_timers.h" // for timer marco.
#endif MPLS

#include "vstacking.h"

#ifndef GI_BOOT
#ifdef M_RING
#include "metro_ring.h"
#endif M_RING
#endif GI_BOOT

#ifdef RATE_LIMIT
extern UINT8 rate_limit_on_interface;
extern UINT8 rate_limit_on_interface_l2;
#endif RATE_LIMIT

#ifdef RATE_LIMIT_VLAN
extern VLAN_ACCESS_TABLE **vlan_access_table;
extern UINT8 rate_limit_on_vlan;
#endif RATE_LIMIT_VLAN

#include "flexauth.h"

#include "parser.h"
#include "acl_cmd1.h"
#include "sw_gi_extern.h"
#include "chassis.h"
#include "sw_pp_basic.h" // CHEETAH_STACKING
#include "vstacking.h" // CHEETAH_STACKING

//FINIL3
#include "register_cache.h"
/* following definations are for software age cycles */
#define FIVE_SECONDS            (50*TIME_UNIT)
#define MAC_AGING_TICK          (TIME_UNIT)             /* currently 100 milliseconds */
#define MAC_AGING_PER_PASS      ((g_station_entries)/(FIVE_SECONDS/MAC_AGING_TICK))

#define MAC_FAST_AGE_INC                20              /* 20 times faster */
#define MAC_NORMAL_AGE_INC              1               /* once each five seconds */
#define MAC_MAX_AGE_CNT                 60              /* assuming 300 seconds def */

/* following definitions for hardware aging */
#define MAC_HW_AGING_TICK       (TIME_UNIT)     /* 100 milliseconds is one tick */
#define MAC_HW_MAX_TICKS        128                     /* 128 ticks */
#define MAX_HW_AGE                      (MAC_HW_MAX_TICKS*MAC_HW_AGING_TICK)
                                                        /* about 12.8 seconds */
#define MAC_HW_AGE_TICKS        10                      /* age in 1 second */
#define MAC_HW_START_TICKS      0                       /* starting value */
#define IP_HW_START_TICS        0                       /* start at zero also */

#define PS1_MASK        0x30
#define PS2_MASK        0x0c
#define FAN1_MASK       0x02
#define FAN2_MASK       0x01

/*Declaring DOM thread*/
pthread_t dom_thread;
UINT32  sim_noage = 0;  /* if 1 no age cycles are issued */
UINT32  g_aging_1000 = 0;       /* count of 1000 pram aged at 1 time */

extern UINT8    g_monitor_mode;
extern UINT8    dma_hw_age_off;                 /* if 1 no hw ageing is performed */
extern UINT32   l2_hw_age_off;                  /* if 1 no hw ageing is performed */
extern void     port_status_poll(void);

extern void sw_fid_periodic_update(void);
extern void sw_router_table_action(UINT8);
extern void update_fake_mac_entry(MAC_STATION *update_mac_entry);
int l2_hitless_vsrp_force_send_timeout=0; //95236

#ifdef SR_SWITCH_ONLY


extern void     sw_igmp_reg_tmr_dist_process(void);
extern void     l3_host_age_entry(UINT8 dma_id, UINT16 index);
#ifndef L3_NO_FILTER
extern void     l3_filter_aging();
#endif
#endif


#ifndef NO_OPTICAL_MONITOR
SV_TIMER_TOKEN_T sv_OpticalMonitor_token = NULL;
SV_TIMER_TOKEN_T sv_OM_SFP_token = NULL;
int OpticalMonitor_sche = 0;
int OpticalMonitor_SFP_timer = 60;
extern void optical_monitor( void );
extern void sfp_optical_monitoring_service( void );

#endif NO_OPTICAL_MONITOR

#ifdef SR_SWITCH_ROUTER
extern void     ip_flow_aging();
extern void fpip_age_forwarding_cache_entry(UINT8, UINT32);
#ifdef __FI_IPSEC_IKE__
INT32 poll_fpga_data_ports_stat ( );
#endif
#endif
#ifdef MCAST_HWAGING
extern void ip_mcast_hw_age_flow_entry(UINT8 dma_id, UINT16 flow_index);
#endif

#ifdef DMA_EMULATE
extern void             dmapram_ctl_exec(UINT8 dma_id);
extern UINT16   dma_test_get_fifo(UINT8 dma_id);
#endif

extern UINT32   dma_scramble(UINT32 addr);
extern void sw_l4_aging(void);
extern void sw_init_igmp_base(void);



#ifndef GI_BOOT
#define MAX_AGE_LOG                                             ( 8 )
#define MAX_AGE_STAT_ENTRIES                    ( MAX_DMA_PER_MODULE * MAX_SLOT+4 )
typedef struct {
        UINT32 value[MAX_AGE_LOG];
        UINT32 idx;
        UINT32 ovfl_num;
        UINT32 aging_word;
} ageStat_t;

ageStat_t ageStat[MAX_AGE_STAT_ENTRIES];

//SV_TIMER_TOKEN_T sv_SysMon_token = NULL;
//#define SYSMON_TIMER 15
extern SYSMON_INFO sm;

int system_aging_diag( UINT8 dma_id )
{
        int i;

#if 0  // Always false
        if (dma_id >= MAX_AGE_STAT_ENTRIES )
                return -1;
#endif

        uprintf("Aging Fifo Stat Info for dma%d:\n", dma_id);
        uprintf("Number of bad indices returned: %d\n", ageStat[DMA_MASTER(dma_id)].ovfl_num);
        uprintf("Last aging index returned: %d\n", ageStat[DMA_MASTER(dma_id)].aging_word);
        uprintf("Bad index buffer dump(current index: %d):\n", ageStat[DMA_MASTER(dma_id)].idx );
        for(i=0; i < MAX_AGE_LOG; i++ ) {
                uprintf("%08x ", ageStat[DMA_MASTER(dma_id)].value[i]);
        }
        uprintf("\n");
        return 0;
}

#endif

extern void mem_dbg_global_1s_timer_processing();

/*--------------------------------------------------------------------------**
**  ip_sw_aging:                                                                        **
*         This procedure is called to do software aging of L3 entries                   **
**--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------**
**  system_aging:                                                                               **
*         This procedure is called to do all aging                                                          **
*       It is called each time the 100 milliseconds timer expires
**--------------------------------------------------------------------------*/

#ifndef NO_USE_L4_CAM_FOR_PRIORITY
UINT32 g_icmp_local_block_count, g_tcp_local_block_count;
#endif NO_USE_L4_CAM_FOR_PRIORITY

enum BOOLEAN sxs100_fx=TRUE;

extern UINT8 g_tick_of_100msec;

extern UINT32                  IntmVecPendingMap;

extern void check_console_idle(void);
extern void errdisable_timer_tick();
extern int l2hitless_hotswap_1s_converge_timer(void);
extern void packet_inerror_sampling ();
extern int lacpctrl_process_timer();
extern int l2hitless_hotswap_100ms_converge_timer(void);
extern void mrp_state_timer(); // Maocheng++: 8.0 single timer,
extern void stp_timer_tick();
extern void mstptimer_tick();
extern int lc_FI_portctrl_100ms_timer(void);
extern void syslog_reset_counter_timer();
extern void ieee_higi_nego_100ms_timer();

#ifdef INCLUDE_MCT
        extern void ccpItcTmrHndlr();
        extern void mct_fsm_process_periodic_timer(void);
        extern void cluster_ccac_process_timer(void);
        extern void mdup_timer_tick(void);
#endif  //INCLUDE_MCT

#ifdef   __PORT_EXTENSION__
    extern void sw_l4_dos_attack_pe_process_timer_event();
#endif  __PORT_EXTENSION__


void system_aging()
{
//      static UINT8 tick_of_100msec; /* TR476469 */
        static INT32 s_one_second_last_time = 0; /* TR476469 */
        UINT32 time_elapsed; /* TR476469 */
        INT32 current_time_reading; /* TR476469 */
        UINT32 one_second_timer = 0;
        static UINT32 ipsec_poll_flag = 0;

#ifdef SR_SWITCH_ROUTER
IP_PORT_DB_ENTRY *portP;
#ifdef __FI_IPSEC_IKE__
#endif /* __FI_IPSEC_IKE__ */
#endif

        UINT16 source_port, total_port, k;
        int i;
        UINT32 temp;

        g_system_aging_cnt ++;
        current_time_reading = g_time_counter; /* TR476469 */
        time_elapsed = current_time_reading - s_one_second_last_time;
        ++g_tick_of_100msec;
        if(time_elapsed >= 10)
        {
                one_second_timer = 1;
                s_one_second_last_time = current_time_reading; /* TR476469 */
        }

        hal_mmu_dyn_buffer_run();


        if(STACK_AM_I_SLAVE) // CHEETAH_STACKING
                goto port_poll_label;


#ifndef GI_BOOT
        vsrp_aware_100ms_timer();
#endif GI_BOOT

        /* New MAC DB API */
        if (g_ma.action != MAC_NO_ACTION)
        {
                if(is_mgmt_active())//NO_HITLESS_SWITCHOVER_SUPPORT
                {
                        if(debug_mac_action_ctrl>=1 || debugGlobal.mac.action_flag)
                        {
                                debug_uprintf("aging_timer. call mac_action_handler()\n");
                                print_ma_debug(0, g_ma.flag, g_ma.action); // premature = 0
                        }
                        mac_action_handler();
                }
        }

#ifdef INCLUDE_MCT
                mdup_timer_tick();
#endif

#ifndef GI_BOOT
#ifdef M_RING
        mrp_utils_check_trunk_state();
#endif M_RING
#endif GI_BOOT

#ifdef RATE_LIMIT
#ifdef SR_SWITCH_ROUTER
        if (rate_limit_on_interface)
#else
        if (rate_limit_on_interface_l2)
#endif SR_SWITCH_ROUTER
        {
                RATE_LIMIT_INFO *rl_next_ptr;

                /* Handle port based rate_limit traffic */
#ifdef SR_SWITCH_ROUTER
                for (portP = ip_port_db_list_head; portP != NULL; portP = portP->next)
                {
                        source_port = portP->port_number;
#else SR_SWITCH_ROUTER
                for (i=0; i<g_hw_info.max_mod; i++)
                {
                        source_port = MAKE_PORTID(i,0);
                        total_port = g_module[i].number_of_ports;
                        for (k = 0; k<total_port; k++, source_port++)
                        {
#endif SR_SWITCH_ROUTER

                if (!IS_PORT_DB_VALID(source_port))
                   continue;

#ifdef SR_SWITCH_ROUTER
                                rl_next_ptr = ip_access_table(source_port, in_rate_limit_table.head);
#else
                                rl_next_ptr = SPTR_PORT_DB(source_port)->port_config.in_rate_limit_table.head;
#endif SR_SWITCH_ROUTER
                                while (rl_next_ptr)
                                {
                                        if (--rl_next_ptr->curr_interval)
                                                goto next_rl1;

                                        rl_next_ptr->curr_interval = rl_next_ptr->cfg_interval;
                                        if (rl_next_ptr->curr_bytes_conform_burst)
                                        {
                                                rl_next_ptr->rec_bytes_conform_burst =
                                                        rl_next_ptr->curr_bytes_conform_burst;
                                                rl_next_ptr->curr_bytes_conform_burst = 0;
                                        }
                                        if (rl_next_ptr->tick_of_2times > 0)
                                        {
                                                rl_next_ptr->tick_of_2times = 0;
                                                if (rl_next_ptr->curr_bytes_exceed_count)
                                                {
                                                        rl_next_ptr->rec_bytes_exceed_count =
                                                                rl_next_ptr->curr_bytes_exceed_count;
                                                        rl_next_ptr->curr_bytes_exceed_count = 0;
                                                }
                                        }
                                        else
                                        {
                                                (rl_next_ptr->tick_of_2times)++;
                                        }


next_rl1:
                                        rl_next_ptr = (RATE_LIMIT_INFO *)rl_next_ptr->next_rate_limit;
                                }

#ifdef SR_SWITCH_ROUTER
                                rl_next_ptr = ip_access_table(source_port, out_rate_limit_table.head);
#else
                                rl_next_ptr = SPTR_PORT_DB(source_port)->port_config.out_rate_limit_table.head;
#endif SR_SWITCH_ROUTER
                                while (rl_next_ptr)
                                {
                                        if (--rl_next_ptr->curr_interval)
                                                goto next_rl2;

                                        rl_next_ptr->curr_interval = rl_next_ptr->cfg_interval;
                                        if (rl_next_ptr->curr_bytes_conform_burst)
                                        {
                                                rl_next_ptr->rec_bytes_conform_burst =
                                                        rl_next_ptr->curr_bytes_conform_burst;
                                                rl_next_ptr->curr_bytes_conform_burst = 0;
                                        }
                                        if (rl_next_ptr->tick_of_2times > 0)
                                        {
                                                rl_next_ptr->tick_of_2times = 0;
                                                if (rl_next_ptr->curr_bytes_exceed_count)
                                                {
                                                        rl_next_ptr->rec_bytes_exceed_count =
                                                                rl_next_ptr->curr_bytes_exceed_count;
                                                        rl_next_ptr->curr_bytes_exceed_count = 0;
                                                }
                                        }
                                        else
                                        {
                                                (rl_next_ptr->tick_of_2times)++;
                                        }

next_rl2:
                                        rl_next_ptr = (RATE_LIMIT_INFO *)rl_next_ptr->next_rate_limit;
                                }
#ifndef SR_SWITCH_ROUTER
                        }
#endif
                }
        }
#endif RATE_LIMIT

#ifdef RATE_LIMIT_VLAN
        if (rate_limit_on_vlan)
        {
                RATE_LIMIT_INFO *rl_next_ptr;
                UINT16 vlan_index;

                /* Handle L2 vlan based rate_limit traffic */
                for (vlan_index = 0; vlan_index < g_sw_sys.max_vlans; vlan_index++)
                {
                        rl_next_ptr = vlan_access_table[vlan_index]->in_rate_limit_table.head;
                        while (rl_next_ptr)
                        {
                                if (--rl_next_ptr->curr_interval)
                                        goto next_rl3;

                                rl_next_ptr->curr_interval = rl_next_ptr->cfg_interval;
                                if (rl_next_ptr->curr_bytes_conform_burst)
                                {
                                        rl_next_ptr->rec_bytes_conform_burst =
                                                rl_next_ptr->curr_bytes_conform_burst;
                                        rl_next_ptr->curr_bytes_conform_burst = 0;
                                }

                                if (rl_next_ptr->tick_of_2times > 0)
                                {
                                        rl_next_ptr->tick_of_2times = 0;
                                        if (rl_next_ptr->curr_bytes_exceed_count)
                                        {
                                                rl_next_ptr->rec_bytes_exceed_count =
                                                        rl_next_ptr->curr_bytes_exceed_count;
                                                rl_next_ptr->curr_bytes_exceed_count = 0;
                                        }
                                }
                                else
                                {
                                        (rl_next_ptr->tick_of_2times)++;
                                }

next_rl3:
                                rl_next_ptr = (RATE_LIMIT_INFO *)rl_next_ptr->next_rate_limit;
                        }

                        rl_next_ptr = vlan_access_table[vlan_index]->out_rate_limit_table.head;
                        while (rl_next_ptr)
                        {
                                if (--rl_next_ptr->curr_interval)
                                        goto next_rl4;

                                rl_next_ptr->curr_interval = rl_next_ptr->cfg_interval;
                                if (rl_next_ptr->curr_bytes_conform_burst)
                                {
                                        rl_next_ptr->rec_bytes_conform_burst =
                                                rl_next_ptr->curr_bytes_conform_burst;
                                        rl_next_ptr->curr_bytes_conform_burst = 0;
                                }
                                if (rl_next_ptr->tick_of_2times > 0)
                                {
                                        rl_next_ptr->tick_of_2times = 0;
                                        if (rl_next_ptr->curr_bytes_exceed_count)
                                        {
                                                rl_next_ptr->rec_bytes_exceed_count =
                                                        rl_next_ptr->curr_bytes_exceed_count;
                                                rl_next_ptr->curr_bytes_exceed_count = 0;
                                        }
                                }
                                else
                                {
                                        (rl_next_ptr->tick_of_2times)++;
                                }

next_rl4:
                                rl_next_ptr = (RATE_LIMIT_INFO *)rl_next_ptr->next_rate_limit;
                        }
                }
        }
#endif RATE_LIMIT_VLAN

#ifndef NO_PORT_LOOPBACK_DETECTION
        loop_detect_100ms_timer();
#endif NO_PORT_LOOPBACK_DETECTION

        {
                //86866,
                lacpctrl_process_timer();

                l2hitless_hotswap_100ms_converge_timer(); // 95236

                mrp_state_timer();

                stp_timer_tick();

                mstptimer_tick();
        }

        syslog_reset_counter_timer();


        //current_time_reading = g_time_counter; /* TR476469 */
        //time_elapsed = current_time_reading - s_one_second_last_time;  /* TR476469 */
        //++g_tick_of_100msec;
        if (one_second_timer) /* TR476469 */
        {
                extern void lagctrl_second_timer(); //autolag, TR000550484
                //s_one_second_last_time = current_time_reading; /* TR476469 */
#ifdef INCLUDE_MCT
                ccpItcTmrHndlr();
                mct_fsm_process_periodic_timer();
                cluster_ccac_process_timer();
#endif //INCLUDE_MCT
                mac_movement_notification_list_aging();

                lagctrl_second_timer(); //autolag, TR000550484
                errdisable_timer_tick();
                l2hitless_hotswap_1s_converge_timer();

                mem_dbg_global_1s_timer_processing();
                packet_inerror_sampling ();
                /* 1 sec event should be put in here */
                cu_rate_counters_1sec_timer();
//              check_console_idle();

#ifdef SR_SWITCH_ROUTER
        if(!IS_SIDEWINDER())
                sw_pp_mac_one_second_timer();


#endif SR_SWITCH_ROUTER

// #ifdef SR_SWITCH_ROUTER
#ifndef NO_USE_L4_CAM_FOR_PRIORITY
                if (rate_control_flags.control_bits.tcp_local)
                {
                        if (!g_tcp_local_discard_mode &&
                                        g_tcp_local_pkts_per_sec > g_tcp_local_burst_per_sec)
                        {
                                /* Discard for next X seconds */
                                g_tcp_local_discard_mode = 1;   /* enter discard mode */
                                g_tcp_local_block_count++;
                                g_tcp_local_lockup_start_time = g_time_counter;
                                send_tcp_local_exceed_burst_trap();
                        }
                        g_tcp_local_pkts_per_sec = 0;

                        if (g_tcp_local_discard_mode)
                        {
                                if ((temp = g_tcp_local_lockup_start_time+g_tcp_local_lockup_period) <=
                                                g_time_counter)
                                {
                                        /* Lockup expires */
                                        /* Allow ICMP to come in now */
                                        g_tcp_local_discard_mode = 0;
                                }
                                else if ((temp-g_time_counter) > g_tcp_local_lockup_period)
                                {       // wrapped
                                        if ((0xffffffff - temp + g_time_counter) >= g_tcp_local_lockup_period)
                                        {
                                                g_tcp_local_discard_mode = 0;
                                        }
                                }

                // h/w based DOS faeture changes
                                if(g_tcp_local_discard_mode==0)
                                {
                                  INT32 rc;
                                  //  block timer expired, so remove 'TCP-SYN Attack Blocker' rule from h/w
                                  rc = sw_pp_l4_acl_delete_dos_attack_blocker_filter(TCP_PROTOCOL,PORT_INDEX_INVALID);
                                  if(rc != CU_OK)
                    trace(INPUT_ACL_TRACE_UTILITY,TRACE_ERROR,
                             "sw_pp_l4_acl_delete_dos_attack_blocker_filter failed.error=%d\n",rc);

                            }

                        }
                }

                if (rate_control_flags.control_bits.icmp_local)
                {
                        if (!g_icmp_local_discard_mode &&
                                        g_icmp_local_pkts_per_sec > g_icmp_local_burst_per_sec)
                        {
                                /* Discard ICMP for next X seconds */
                                g_icmp_local_discard_mode = 1;  /* enter icmp discard mode */
                                g_icmp_local_block_count++;
                                g_icmp_local_lockup_start_time = g_time_counter;
                                send_icmp_local_exceed_burst_trap();
                        }
                        g_icmp_local_pkts_per_sec = 0;

                        if (g_icmp_local_discard_mode)
                        {
                                if ((temp = g_icmp_local_lockup_start_time+g_icmp_local_lockup_period) <=
                                                g_time_counter)
                                {
                                        /* Lockup expires */
                                        /* Allow ICMP to come in now */
                                        g_icmp_local_discard_mode = 0;
                                }
                                else if ((temp-g_time_counter) > g_icmp_local_lockup_period)
                                {       // wrapped
                                        if ((0xffffffff - temp + g_time_counter) >= g_icmp_local_lockup_period)
                                        {
                                                g_icmp_local_discard_mode = 0;
                                        }
                                }
                // h/w based DOS faeture changes
                                if(g_icmp_local_discard_mode==0)
                                {
                                  INT32 rc;
                                  //  block timer expired, so remove 'ICMP Attack Blocker' rule from h/w
                                  rc = sw_pp_l4_acl_delete_dos_attack_blocker_filter(ICMP_PROTOCOL,PORT_INDEX_INVALID);
                                  if(rc != CU_OK)
                    trace(INPUT_ACL_TRACE_UTILITY,TRACE_ERROR,
                             "sw_pp_l4_acl_delete_dos_attack_blocker_filter failed.error=%d\n",rc);

                            }

                        }
                }

                /* Handle port based transit traffic */
#ifdef SR_SWITCH_ROUTER
#ifdef __PORT_EXTENSION__
        if(STACK_AM_I_PE)
            sw_l4_dos_attack_pe_process_timer_event();
#endif __PORT_EXTENSION__
                for (portP = ip_port_db_list_head; portP != NULL; portP = portP->next)
                {
                        source_port = portP->port_number;
                        if (!IS_IP_PORT_DB_VALID(source_port))
                                continue;
#else SR_SWITCH_ROUTER
                for (i = 0; i < g_hw_info.total_ports; i++)
                {
                        source_port = sw_swport_list[i];
#endif SR_SWITCH_ROUTER
                        if (ip_access_table(source_port, tcp_transit_attack))
                        {
                                if (!ip_access_table(source_port, tcp_transit_discard_mode) &&
                                                ip_access_table(source_port, tcp_transit_pkts_per_sec) >
                                                ip_access_table(source_port, tcp_transit_burst_per_sec))
                                {
                                        /* Discard for next X seconds */
                                        ip_access_table(source_port, tcp_transit_discard_mode) = 1;     /* enter discard mode */
                                        ip_access_table(source_port, tcp_block_count)++;
                                        ip_access_table(source_port, tcp_transit_lockup_start_time) = g_time_counter;
                                        send_tcp_transit_exceed_burst_trap(source_port);
                                }
                                ip_access_table(source_port, tcp_transit_pkts_per_sec) = 0;

                                if (ip_access_table(source_port, tcp_transit_discard_mode))
                                {
                                        if ((temp = ip_access_table(source_port, tcp_transit_lockup_start_time)+
                                                        ip_access_table(source_port, tcp_transit_lockup_period)) <=
                                                                g_time_counter)
                                        {
                                                ip_access_table(source_port, tcp_transit_discard_mode) = 0;
                                        }
                                        else if ((temp-g_time_counter) > ip_access_table(source_port, tcp_transit_lockup_period))
                                        {       // wrapped
                                                if ((0xffffffff - temp + g_time_counter) >= ip_access_table(source_port, tcp_transit_lockup_period))
                                                {
                                                        ip_access_table(source_port, tcp_transit_discard_mode) = 0;
                                                }
                                        }
                // h/w based DOS faeture changes
                                        if(ip_access_table(source_port, tcp_transit_discard_mode)==0)
                                        {
                                          INT32 rc;
                                          // block timer expired, so remove 'TCP-SYN Attack Blocker' rule from h/w
                                          rc = sw_pp_l4_acl_delete_dos_attack_blocker_filter(TCP_PROTOCOL,source_port);
                                          if(rc != CU_OK)
                        trace(INPUT_ACL_TRACE_UTILITY,TRACE_ERROR,
                                 "sw_pp_l4_acl_delete_dos_attack_blocker_filter on Port %P failed.error=%d\n",source_port,rc);

                                        }

                                }
                        }

                        if (ip_access_table(source_port, icmp_transit_attack))
                        {
                                if (!ip_access_table(source_port, icmp_transit_discard_mode) &&
                                                ip_access_table(source_port, icmp_transit_pkts_per_sec) >
                                                ip_access_table(source_port, icmp_transit_burst_per_sec))
                                {
                                        /* Discard for next X seconds */
                                        ip_access_table(source_port, icmp_transit_discard_mode) = 1;    /* enter discard mode */
                                        ip_access_table(source_port, icmp_block_count)++;
                                        ip_access_table(source_port, icmp_transit_lockup_start_time) = g_time_counter;
                                        /* Turn off ICMP thru hardware */
                                        gi_set_protocol_table_for_ICMP_attack(source_port,  L45_DROP);
                                        send_icmp_transit_exceed_burst_trap(source_port);
                                }
                                ip_access_table(source_port, icmp_transit_pkts_per_sec) = 0;

                                if (ip_access_table(source_port, icmp_transit_discard_mode))
                                {
                                        if ((temp = ip_access_table(source_port, icmp_transit_lockup_start_time)+
                                                        ip_access_table(source_port, icmp_transit_lockup_period)) <=
                                                        g_time_counter)
                                        {
                                                /* Lockup expires */
                                                /* Allow to come in now */
                                                ip_access_table(source_port, icmp_transit_discard_mode) = 0;
                                                /* Turn on ICMP thru hardware */
                                                gi_set_protocol_table_for_ICMP_attack(source_port,  L45_INTERVENTION);
                                        }
                                        else if ((temp-g_time_counter) > ip_access_table(source_port, icmp_transit_lockup_period))
                                        {       // wrapped
                                                if ((0xffffffff - temp + g_time_counter) >= ip_access_table(source_port, icmp_transit_lockup_period))
                                                {
                                                        ip_access_table(source_port, icmp_transit_discard_mode) = 0;
                                                        /* Turn on ICMP thru hardware */
                                                        gi_set_protocol_table_for_ICMP_attack(source_port,  L45_INTERVENTION);
                                                }
                                        }
                // h/w based DOS faeture changes
                                        if(ip_access_table(source_port, icmp_transit_discard_mode)==0)
                                        {
                                          INT32 rc;
                                          //  block timer expired, so remove 'ICMP Attack Blocker' rule from h/w
                                          rc = sw_pp_l4_acl_delete_dos_attack_blocker_filter(ICMP_PROTOCOL,source_port);
                                          if(rc != CU_OK)
                        trace(INPUT_ACL_TRACE_UTILITY,TRACE_ERROR,
                                 "sw_pp_l4_acl_delete_dos_attack_blocker_filter on Port %P failed.error=%d\n",source_port,rc);

                                        }

                                }
                        }
                }
#endif NO_USE_L4_CAM_FOR_PRIORITY
// #endif SR_SWITCH_ROUTER
//              tick_of_100msec = 0; /* TR476469 */
        } //if (++tick_of_100msec >= 10)

        /* the ageing fifo manager is also called from the main loop because
        the rate here is totally inadequate. The fifo can hold only upto
        4 entries and hence if there are 8k entries that can take upto 200 seconds
        for the age cycle to complete.
        Estimated time to complete hw age cycle
        Entries         time
        8k                      200 seconds
        4k                      100 seconds
        2k                       50 seconds
        1k                       25 seconds
        To achieve fast ageing of cam entries when the number of entries is small
        we might like to poll the ageing fifo faster, maybe every 10milliseconds.
        The other thing that works in our favour is that as the cam-size increases
        the need to fast ageing diminishes, because cam resource constraint is
        lighter.

        - we need to distribute these tasks further so that not all happen
        on each tick of 100 milliseconds
        */

#ifdef SR_SWITCH_ONLY
        /* added for enahcned l3 support */
        if ((g_time_counter & 0xf) == 0) /* g_time_counter is at 16x */
        {

//              sw_update_bm_ctr();

                if (g_ip_switch_enabled) {
                        /* reset number of cam insertions in this 1600 millisec period */
                        g_l3_insert_cnt = 0;

                        if (!g_l3_traffic_learn) {
#ifdef SR_IP_SWITCH
                                sw_enable_l3_learning();
#endif
                        }
                }
        }
#endif

        /******  Aging for ACL logging session Fan *****/
        if( (g_time_counter%ACL_LOGGING_AGING_TIME == 0) &&
                (g_time_counter>=ACL_LOGGING_AGING_TIME)
          )
        {
                acl_log_entry_aging();
        }

#ifndef NO_PROTO_VLANS
        if (g_sw_sys.l3_vlan_mode) {

                        sw_vlan_periodic();

        }
#endif

        if (!(g_time_counter%10))
        {
                UINT32 slot, i, port;

                g_sw_sys.curr_brd = 0;
                g_sw_sys.curr_mcast = 0;
                g_sw_sys.curr_unknown_unicast = 0;

                for (slot=0; slot < g_hw_info.max_mod; slot++)
                {
                        if (MODULE_IS_GOOD(slot))
                        {
                                port = g_module[slot].start_sw_port;
                                for (i=0; i<g_module[slot].number_of_ports; i++)
                                {
                                        if(IS_PORT_DB_VALID(port))
                                                {
                                        (SPTR_PORT_DB(port))->port_config.curr_brd = 0;
                                        (SPTR_PORT_DB(port))->port_config.curr_mcast = 0;
                                        (SPTR_PORT_DB(port))->port_config.curr_unknown_unicast = 0;
                                                }
                                        port++;
                                }
                        }
                }
                /* CONSOLE lock-up fix, see Int_uart.c for detail */
                if (io_cb[0].escape_mode)
                        check_console_escape_mode(g_time_counter/10);
        }

//#ifdef MAC_FILTER
        // removing aging of the l2_session_table. don't see the use of this anywere and have been reviewed by multiple experts.
        // was part of the flow based filters.
        //l2_session_aging();
//#endif MAC_FILTER

        if (g_sw_sys.max_sessions_curr != 0)
                sw_l4_session_ager(g_time_counter);  // should be called every 100 msec

#ifdef __IPV6_ACL_LOG__
        sw_ipv6_l4_session_ager();
#endif __IPV6_ACL_LOG__

#ifdef SR_SWITCH_ONLY
#ifndef L3_NO_FILTER
        l3_filter_aging();
#endif
#endif

        if (!(g_time_counter&0xff)) /* 25 seconds */
        {
                trace_l2_25_second_timer(); // 54732

#ifdef SR_SWITCH_ROUTER
                if (rtr_use_acl)
                        sw_l4_update_acl_change_flag(NULL);
#else
                sw_l4_update_acl_change_flag(NULL);
#endif SR_SWITCH_ROUTER

#ifdef RATE_LIMIT
                sw_rate_limit_reset_acl_change_flag(NULL);
#endif RATE_LIMIT
        }

#ifdef SR_SWITCH_ROUTER
        if (!rtr_use_acl)
                ip_flow_aging();                                        /* wwl: flow cache aging for NetIron */
#endif /* SR_SWITCH_ROUTER */

#ifdef IP_POLICY_ROUTING
        ip_policy_flow_aging();
#endif IP_POLIYC_ROUTING

#ifdef NI_SLB
        ni_l4_server();
#endif

#ifdef __PLGV2__
      plgTimerEvent(10);
#endif  __PLGV2__

#ifndef NO_LINK_KEEPALIVE
        l2ka_periodic(g_time_counter);
#endif NO_LINK_KEEPALIVE
        mac_notification_timer(g_time_counter);

#ifdef FI_LOAM
        /* Link OAM periodic timer */
    link_oam_periodic_timer(g_time_counter);
#endif

#ifdef MPLS
        if (mpls.application_enabled)
        {
                MPLS_SV_EXECUTE_TIMER_100MS();
                mpls_timer_100ms();
        }
#endif MPLS

#ifdef __LINK_CFG_AT_INTERFACE_LEVEL__
        if(sxs100_fx==TRUE)
        {
                sxs100_fx = FALSE;
                for (i=0; i<g_hw_info.total_config_ports; i++) {
                        source_port = sw_config_port_list[i];
                        if ( IS_PORT_DB_VALID(source_port) )
                        {
                                if ( is_pp_link_100_fx_port(source_port) )
                                {
                                        /* NEC has to disable first or won't UP */
                                        port_disable_cmd(source_port,PORT_DIS_by_SYS); //483612

                                        WAIT_LOOP(16500);
                                        port_command(source_port, PORT_ENABLE, 0L);
                                }
                        }
                }
        }
#endif __LINK_CFG_AT_INTERFACE_LEVEL__

        /* l3_host_aging done in arp one second timer */
port_poll_label:
        {

                if (STACK_AM_I_STANDBY)
                {

#ifdef FI_LOAM
                        /* Link OAM periodic timer */
                    link_oam_periodic_timer(g_time_counter);
#endif

                        loop_detect_100ms_timer();
                        lacpctrl_process_timer();
                        mrp_state_timer();
                        /* BUG: 298500, 298611
                        *    link-keepalive timer need to run on standby
                        */
                        l2ka_periodic(g_time_counter);
                        stp_timer_tick();
                        mstptimer_tick();

                        if(one_second_timer)
                        {

                                extern void lagctrl_second_timer(); //autolag, TR000550484
                                lagctrl_second_timer(); //autolag, TR000550484
                                errdisable_timer_tick();
                                mem_dbg_global_1s_timer_processing();
                                if(write_shadow_flag_get()) // STACK_AM_I_STANDBY is true
                                {       // This is in timer task
                                        if(g_stacking_write_to_shadow_memory_set_time == 0)
                                                g_stacking_write_to_shadow_memory_set_time = sv_get_time();
                                }
                                else
                                        g_stacking_write_to_shadow_memory_set_time = 0;
                        }
                }
                if(STACK_AM_I_MEMBER)
                {
#ifdef __UDLD_DISTRIBUTED_SUPPORT__
                        l2ka_periodic(g_time_counter);
#endif __UDLD_DISTRIBUTED_SUPPORT__
#ifdef FI_LOAM
                        /* Link OAM periodic timer */
                    link_oam_periodic_timer(g_time_counter);
#endif
                        if(one_second_timer)
            {
                mem_dbg_global_1s_timer_processing();
            }

                }

                lc_FI_portctrl_100ms_timer();
                hal_sw_pp_mac_100ms_timer();
        }

        {
                port_status_poll();
                if(common_stk_link_negotiation_needed())
                    ieee_higi_nego_100ms_timer();
        }

                if (!(g_time_counter%10))
                {
                        mcast_snoop_second_timer(IP_IPV4_AFI); // IGMPV3_SNOOP 67062
                        // since igmp/mld code is common for pimsm , calling pimsm from independent place
                        // it should get more agnostic naming , once igmp/mld code is merged.
                        pimsm_snoop_second_timer(IP_IPV4_AFI);// 67062

#ifdef __MLD__ // MLDV2_SNOOP, keep this as marker for easy porting
                        mcast_snoop_second_timer(IP_IPV6_AFI); // 65632
                        pimsm_snoop_second_timer(IP_IPV6_AFI);
#endif __MLD__
                }

#ifdef __MLD__ // MLDV2_SNOOP, keep this as marker for easy porting]
                if (ip6.mld.proxy_entry_pool && ip6.mld.proxy_entry_pool->allocated_number)
                {
                        mcast_snoop_proxy_timer(IP_IPV6_AFI); // 66620
                }
#endif __MLD__
// IGMPV3_SNOOP 67062
                if (ip.igmp.proxy_entry_pool && ip.igmp.proxy_entry_pool->allocated_number)
                {
                        mcast_snoop_proxy_timer(IP_IPV4_AFI); // 66620
                }

#ifdef SR_SWITCH_ROUTER
#ifdef __FI_IPSEC_IKE__
         if ( IS_SPATHA())
             if(one_second_timer)
             {   /* poll at 2 sec */
                 if ( ipsec_poll_flag )
                 {
                     poll_fpga_data_ports_stat ();
                     ipsec_poll_flag = 0;
                 }
                 else
                     ipsec_poll_flag = 1;
              }
#endif /* __FI_IPSEC_IKE__ */
#endif /* SR_SWITCH_ROUTER */

#ifdef SR_SWITCH_ROUTER

        if(one_second_timer)
        {
                mcast_l3_entry_aging ();
        }
#endif

        remove_storm_prevention_partially();
        if(g_stacking.stacking_enable == STACKING_STATE_ENABLED)
                stacking_100ms_system_time();
        handle_elapse_time(0); // 0 is from system_aging

#ifdef __MACSEC_ENABLE__
    hal_msec_interrupt_polling();
#endif __MACSEC_ENABLE__
}

extern void poll_backplane(int);

#ifndef NO_TRUNKING_THRESHOLD
extern UINT8 g_trunk_threshold;
extern void trunk_check_all_trunks_threshold(void);
#endif NO_TRUNKING_THRESHOLD

#ifdef SPATHA
#ifndef FI_EEE_MGMT
extern void eee_power_calc_init(void);
#endif
#endif
/*--------------------------------------------------**
** name:        system_aging_init                       **
**      This procedure initializes the aging sub-system **
**--------------------------------------------------*/
void system_aging_init()                        /* -LC */
{
        /* initialize the aging process */
        g_sw_sys.sv_aging_token = sv_set_timer(HUNDRED_MILLISEC, REPEAT_TIMER, system_aging,0);

#ifndef NO_TRUNKING_THRESHOLD
        {
                extern SV_TIMER_TOKEN_T g_trunk_threshold_timer;
                if (g_trunk_threshold  && ( NULL == g_trunk_threshold_timer ) ) {
                        g_trunk_threshold_timer = sv_set_timer(SECOND, REPEAT_TIMER, trunk_check_all_trunks_threshold, 0);
                }
        }
#endif NO_TRUNKING_THRESHOLD

#ifndef NO_OPTICAL_MONITOR
        if(IS_FI_BCM() && !IS_TANTO()) {
                optical_monitor_init();
        }
        else if(!IS_TANTO())
        {
                optical_monitor_init();
                sv_OpticalMonitor_token = sv_set_timer(OpticalMonitor_sche*SECOND, REPEAT_TIMER, optical_monitor, 0 );
                sv_OM_SFP_token = sv_set_timer(OpticalMonitor_SFP_timer*10*SECOND, REPEAT_TIMER, sfp_optical_monitoring_service, 0 );
        }
#endif NO_OPTICAL_MONITOR

#ifdef JC_LOCK_UP_DETECT

#ifdef BACKPLANE_POLLING
        /* initialize the backplane polling process for non-ipc cpu module */
        sv_set_timer(SECOND, REPEAT_TIMER, poll_backplane, 0);
#endif BACKPLANE_POLLING

        jc_lock_start();

#endif JC_LOCK_UP_DETECT

#if 0
        /*
         * rlau
         * mac_bridge_timer() will be called periodically through
         * stp_timer_tick(). Therefore, I don't think that we need
         * to call mac_bridge_timer() in here. Besides, passing 0
         * as parameter to mac_bridge_timer() will crash the system.
         */

        /* start Spanning Tree timer */
        if (g_sw_sys.stp_enabled)
        {
        /*      timer manager for spanning tree protocol */
                g_stp_hello_timer_token = sv_set_timer(STP_TIME, REPEAT_TIMER, mac_bridge_timer, 0);
        }
#endif
/*
        {
          extern int errdisable_init(void);
          errdisable_init();

        }*/

/* Initialize the SysMon Timer  */
        if(!sm.sysMon_timer)  // if startup configuration has sysmon timer set, it will not reset to default (3minutes)
                sm.sysMon_timer = 3;
        sm.SysMon_sv_token = sv_set_timer_event(sm.sysMon_timer*60*SECOND, REPEAT_TIMER, system_monitoring, 0,"sysMon Startup");
        sm.sysMon_enable = TRUE;
#ifdef SPATHA
#ifndef FI_EEE_MGMT
        eee_power_calc_init();
#endif
#endif
}

extern void send_fanStatus_trap(UINT8 index, char *description, UINT16 trap_id);
#define FAN_RETRY_COUNT 3

#if     !defined(GI_BOOT)
int halt_after_reset = 0;
int temp_above_shutdown = 0;
#endif

static void check_pwr_status(
                UINT32 status,
                UINT32 mask,
                UINT8 pwr_number,
                UINT8 *pwr_description)
{
        if ((mask & ~g_sw_sys.chassis_mask) != mask)
                return;

        if ((status & mask) == 0)
        {
                /* power supply failed */
                send_powerSupply_trap(pwr_number, pwr_description);
        }
}

static UINT8 check_fan_status(
                UINT32 status,
                UINT32 mask,
                UINT8 fan_number,
                UINT8 retry,
                UINT8 *fan_description)
{
        if ((mask & ~g_sw_sys.chassis_mask) != mask)
                return 0;

        if ((status & mask) == 0)
        {
                /* fan bad */
                if (++retry >= FAN_RETRY_COUNT)
                {
                        send_fanStatus_trap(fan_number, fan_description, TRAP_CHAS_FAN_FAILURE);
                        retry = 0;
                }
        }
        else
        {
                /* fan OK */
                retry = 0;
                send_fanStatus_trap(fan_number, fan_description, TRAP_CHAS_FAN_NORMAL);
        }
        return retry;
}

/* Poll the chassis status (e.g. power supply) in every 60 seconds. */
static UINT32
sw_poll_chassis(UINT32 state)
{
        static SV_TIMER_TOKEN_T sv_poll_chassis_token = (SV_TIMER_TOKEN_T) 0;
        static UINT8 fan_retry[6];
        static SV_TIMER_TOKEN_T stk4802_poll_fan_token = (SV_TIMER_TOKEN_T) 0;

        UINT32 pwr_fan_status;
        UINT8 i;
        CHASSIS_PROFILE_DATA       chassisProfile;
        CHASSIS_FAN_STATUS         fanStatus;

        switch (state)
        {
        case 0:
                /* To stop polling, user calls sw_poll_chassis(0). */

                /* Reset state back to 1 for next call. */
                state = 1;
                if (sv_poll_chassis_token != (SV_TIMER_TOKEN_T) 0)
                {
                        sv_cancel_timer(sv_poll_chassis_token);
                        sv_poll_chassis_token = (SV_TIMER_TOKEN_T) 0;
                }
                break;

        case 1:
                /* To start polling, user calls sw_poll_chassis(1). */

                /* Change state to 2 to check chassis status periodically. */
                state = 2;
                sv_poll_chassis_token = sv_set_timer(
                                                                        (g_sw_sys.chassis_poll_time * SECOND),
                                                                        REPEAT_TIMER,
                                                                        (void (*)()) sw_poll_chassis,
                                                                        state);
                for (i=0; i < 6; i++)
                        fan_retry[i] = 0;


                break;

        case 2:
                break;
        }
        return state;
}

void
sw_set_chassis_poll_time(UINT16 poll_time)
{
        g_sw_sys.chassis_poll_time = poll_time;

        chassisSetEventPollTimeSec(poll_time);
}

extern void tanto_dom_init(PORT_ID);

#define MAX_TANTO_BASE_MOD_PORTS       48
#define FLEX_MOD_40G_QSFP_PORTS       2
#define FLEX_MOD_10G_SFPP_PORTS       4
#define FLEX_MOD_100G_QSFP_PORTS     1
#define REAR_MOD_40G_PORTS            4
#define REAR_MOD_100G_PORTS           2

extern int om_enabled_port_list[];

void traverse_tanto_dom_init()
{
	uprintf("INSIDE traverse_tanto_dom_init\r\n");
	int lport,slot;
	STACK_ID stackId;
	stackId = MY_BOOTUP_STACK_ID;
	for(slot = 0; slot < (MAX_LOCAL_SLOT - 1); slot++){
		int module = MAKE_MODULE_ID(stackId, slot);

		switch(get_tanto_module_type(stackId, module)){
						
			case GS_TANTO_STACK_BASE_48GF:
									
				for(lport = 0; lport < MAX_TANTO_BASE_MOD_PORTS; lport++){
					
					PORT_ID port = MAKE_PORTID(module, lport);
					if (om_enabled_port_list[PORT_TO_LOCAL_PORT(port)] != 0) {
						tanto_dom_init(port);	
					}
				}
				break;
			
			case GS_TANTO_STACK_MOD_4XGF:
									
				for(lport = 0; lport < FLEX_MOD_10G_SFPP_PORTS; lport++){
					
					PORT_ID port = MAKE_PORTID(module, lport);
					if (om_enabled_port_list[PORT_TO_LOCAL_PORT(port)] != 0) {
						tanto_dom_init(port);	
					}
				}
				break;

			case GS_TANTO_STACK_MOD_2QXG:
									
				for(lport = 0; lport < FLEX_MOD_40G_QSFP_PORTS; lport++){
					
					PORT_ID port = MAKE_PORTID(module, lport);
					if (om_enabled_port_list[PORT_TO_LOCAL_PORT(port)] != 0) {
						tanto_dom_init(port);	
					}
				}
				break;

			case GS_TANTO_STACK_MOD_1HGF:
									
				for(lport = 0; lport < FLEX_MOD_100G_QSFP_PORTS; lport++){
					
					PORT_ID port = MAKE_PORTID(module, lport);
					if (om_enabled_port_list[PORT_TO_LOCAL_PORT(port)] != 0) {
						tanto_dom_init(port);	
					}
				}
				break;

			case GS_TANTO_STACK_MOD_4QXG:
									
				for(lport = 0; lport < REAR_MOD_40G_PORTS; lport++){
					
					PORT_ID port = MAKE_PORTID(module, lport);
					if (om_enabled_port_list[PORT_TO_LOCAL_PORT(port)] != 0) {
						tanto_dom_init(port);	
					}
				}
				break;

			case GS_TANTO_STACK_MOD_2HGF:
									
				for(lport = 0; lport < REAR_MOD_100G_PORTS; lport++){
					
					PORT_ID port = MAKE_PORTID(module, lport);
					if (om_enabled_port_list[PORT_TO_LOCAL_PORT(port)] != 0) {
						tanto_dom_init(port);	
					}
				}
				break;

			default:
				break;

		}
	}

}


void tanto_om_scheduler_init()
{
	uprintf("INSIDE tanto_om_scheduler_init\r\n");

		if(IS_TANTO()){
			//INDRA:: daemonize and fork out a new process
			//call optical_monitor_init() inside a daemonize busy loop.
			//The local host server and the daemon will be connected through a socket 
			//and UDP will be established. Essentially both the servers will be in sleeping
			//state and whenever optical_monitoring will be triggered through CLI then both 
			//the servers should wake up.
			//
			//Once local server thread wakes up then it sends the locally populated g_port_db()/
			//g_hw_info/g_moduole and other related data structures to the client which would also be in woken up state.
			//Once client recieves this info it should call optical_monitor_init() and other relates call
			//flows which is hit by CLI.
			//
			//Once CLIENT has all the necessary infos then it operates on the sfp/qsfp eeprom and stores it inside 
			//sfp_monitor_threshold_string and SFP_MONITOR for optic thresold and optic power respectively. This information is local to the 
			//CLIENT and server needs not to know about this since it just hand shakes with CLIENT to propagate the FI global
			//data  and to perform the operation.
			//
			//when show operation is invoked again then again server/client hand shakes and it operates on the display data saved in
			//client process running under daemon
			
			//DOM thread has to lie on a separate thread and would be running on it.
			
			extern int socket_init();
			/*
			pthread_attr_t attr;
			pthread_attr_init(&attr);
			pthread_attr_setstacksize(&attr,1024*1024);
			int status;
			uprintf("Creating DOM Thread\n");

			status=pthread_create(&dom_thread,&attr,(void*)&socket_init,NULL);
			*/
			int status = socket_init();
			if (status!=0) { 
				uprintf("Failed to create DOM Threads with Status:%d\n",status);
			} else {
			//	traverse_tanto_dom_init();
			}
			


		}
}

