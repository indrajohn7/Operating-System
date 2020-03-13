/*
 *	Copyright (C) 2001 Foundry Networks Inc.
 *	Unpublished - rights reserved under the Copyright Laws of the
 *	United States.  Use, duplication, or disclosure by the
 *	Government is subject to restrictions as set forth in
 *	subparagraph (c)(1)(ii) of the Rights in Technical Data and
 *	Computer Software clause at 252.227-7013.
 */
#include"dom_utils.h"
#include "sw.h"
#include "debugstruct.h"
#include "kmalloc.h"
#include "hw_cmds.h"
#include "chassis.h"
#include "chassis_stk_SW.h"
#include "math.h"
#include "sw_pp_link.h"
#include "optical_monitor.h"
#include "platform_types.h"
#include "rel_ipc.h"
#include "sw_seeq.h"
#include "fluffy_phy_driver.h"

extern UINT32 dm_debug_mask;
extern DEBUG_GLOBAL debugGlobal;
extern int dom_down_port_on_off_set;
extern int dom_non_brocade_on_off_set;

int mw2dbm (UINT16 mwreg, UINT32 * val);

int  om_enabled_port_list[MAX_LOCAL_PORTS];
void tanto_sfp_optical_monitoring (PORT_ID lport,char* buf);
void tanto_sfp_monitor_data_init(PORT_ID lport, char* buffer);
int tanto_sfp_optical_monitoring_service(PORT_ID sfp_port);
int tanto_optical_monitoring(PORT_ID);
static unsigned int icx7650_get_sfp_monitor_from_port(PORT_ID port);
static void optical_monitior_log_msg (enum SYS_LOG_MSG_TYPE log_msg_type, int module_port_index);
static int is_sfpp_capable_of_optical_monitoring (PORT_ID module_port_index);


char* const sfp_monitor_flag_string[] = {
  "Latched high Temperature alarm",
  "Latched low Temperature alarm",
  "Latched high Supply Voltage alarm",
  "Latched low Supply Voltage alarm",
  "Latched high TX Bias Current alarm",
  "Latched low TX Bias Current alarm",
  "Latched high TX Power alarm",
  "Latched low TX Power alarm",
  "Latched high RX Power alarm",
  "Latched low RX Power alarm",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Latched high Temperature warning",
  "Latched low Temperature warning",
  "Latched high Supply Voltage warning",
  "Latched low Supply Voltage warning",
  "Latched high TX Bias Current warning",
  "Latched low TX Bias Current warning",
  "Latched high TX Power warning",
  "Latched low TX Power warning",
  "Latched high RX Power warning",
  "Latched low RX Power warning",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved",
  "Reserved"
};

char* const sfp_monitor_threshold_string[] = {
  "Temperature High alarm            ",
  "Temperature Low alarm             ",
  "Temperature High warning          ",
  "Temperature Low warning           ",
  "Supply Voltage High alarm         ",
  "Supply Voltage Low alarm          ",
  "Supply Voltage High warning       ",
  "Supply Voltage Low warning        ",
  "TX Bias High alarm                ",
  "TX Bias Low alarm                 ",
  "TX Bias High warning              ",
  "TX Bias Low warning               ",
  "TX Power High alarm               ",
  "TX Power Low alarm                ",
  "TX Power High warning             ",
  "TX Power Low warning              ",
  "RX Power High alarm               ",
  "RX Power Low alarm                ",
  "RX Power High warning             ",
  "RX Power Low warning              "
};

struct om_sched_slot {
  int slot;
  int enable;               /* enabled to schedule? */
  int num_ports;
  int num_ports_per_cycle;

  int serv_port;            /* next port to serve */
  int num_port_served;      /* number of ports served */

};

/* om scheduler schedules which slot/port to serve */
struct om_sched {
  char name[16];
  int num_slots;                /* total number of slots */
  struct om_sched_slot slots[MAX_LOCAL_SLOT];  /* for all slots */

  int serv_slot;                /* slot to serve */
  int num_slot_served;          /* number of slots served */   
};

int g_uprintf_dest_save ;
static SFP_MONITOR sfp_monitor[MAX_SYS_UNIT_NUM + 1][MAX_NUM_SFP_MONITOR] = { {0} };
DOM_STACK_CONFIG dom_config[MAX_SYS_UNIT_NUM + 1][MAX_NUM_SFP_MONITOR] = {{0,}, };
static struct om_sched service;
static struct om_sched monitor;

static void op_zap_per_port_data (PORT_ID lport);
static int sfp_get_calibration_measure(PORT_ID lport);
static int qsfp_get_calibration_measure(PORT_ID lport);
int sw_cu_get_calibration_measure(PORT_ID l_domport);

extern SV_TIMER_TOKEN_T sv_OpticalMonitor_token;
extern SV_TIMER_TOKEN_T sv_OM_SFP_token;
extern UINT16 optical_monitor_interval;
void optical_monitor (void);
//UINT32 dp_pp_sfp_read_internal (PORT_ID port, UINT8 * data, UINT32 length);
int sxs_sfp_read (PORT_ID port, UINT8 offset, UINT8 * data, int length);
void send_port_optical_msg_type(UINT8 *msg, enum SYS_LOG_MSG_TYPE type);
int is_module_exist(int slot) ;
int is_sfp_channel_ok (PORT_ID port);
void hal_optical_monitor(void);
void hal_sfp_optical_monitoring_service(void);

/*static functions */
static void sfp_optical_monitoring_service (void);
static void show_sfp_optic_power_helper(PORT_ID port, qsfpp_port_optic_t *port_optic, 
                            int index, int *valid_rx1, int *valid_bias1);
static void display_40G_optic_power_head(UINT8 media_type);
static void show_40G_optic_power_helper(PORT_ID port, qsfpp_port_optic_t *port_optic, 
                            int index, int invalid_rx1, int invalid_bias1);
static void sfp_monitor_data_init(PORT_ID lport);
static unsigned int get_sfp_monitor_from_port(PORT_ID port);
static unsigned int get_dom_config_from_port(PORT_ID port);
static unsigned int get_dom_config_from_sfp_monitor(int sfp_monitor);
static int sfp_media_read(PORT_ID port, UINT8 addr, UINT8 offset, UINT32 length, UINT8 *buf);
static int qsfp_read_monitoring_flags(PORT_ID lport,unsigned char * buf);
static int sfp_read_monitoring_flags(PORT_ID lport,unsigned char * buf);
static PORT_ID get_port_from_sfp_monitor(int stack_id, int sfp_monitor);
static int qsfp_media_page_read(PORT_ID port, UINT8 addr, UINT8 page, UINT8 length, UINT8 *buf);
static int optical_monitor_data_ready(PORT_ID lport);
static int get_num_sfp_monitors(int stackId);
static enum SFP_MONITOR_DEVICE_TYPE is_sfp_capable_of_optical_monitoring (PORT_ID port);
extern int is_qsfp_optictype_on_x40g (PORT_ID port);
extern int is_sfp_sfpp_optictype_on_x40g (PORT_ID port);
static int is_qsfp_optic_dom_supported (PORT_ID port);

/*** chassis dependent section:  sidewinder  ***/
#define MAX_SFP_ICX7750_48F  48
#define MAX_QSFP_ICX7750_48F 12

#define MAX_SFP_ICX7750_48C  0
#define MAX_QSFP_ICX7750_48C 12

#define MAX_SFP_ICX7750_26Q  0
#define MAX_QSFP_ICX7750_26Q 32

static int icx7750_dom_init(void);
static int icx7750_om_scheduler_init(struct om_sched *p_sched);
static void icx7750_om_init(void);
static PORT_ID icx7750_get_port_from_sfp_monitor(int stackId, int sfp_monitor);
static unsigned int icx7750_get_sfp_monitor_from_port(PORT_ID port);
static int icx7750_get_num_sfp_monitors(int stackId);
/*** end of sidewinder  ***/



/*** chassis dependent section:  spatha  ***/
/* max SFP/SFPP/QSFPP per platorm */
#define MAX_SFP_ICX7450_48F   60   
#define MAX_SFP_ICX7450_24    12   
#define MAX_SFP_ICX7450_48    12    

static int icx7450_dom_init(void);
static int icx7450_om_scheduler_init(struct om_sched *p_sched);
static void icx7450_om_init(void);
static PORT_ID icx7450_get_port_from_sfp_monitor(int stackId, int sfp_monitor);
static unsigned int icx7450_get_sfp_monitor_from_port(PORT_ID port);
static int icx7450_get_num_sfp_monitors(int stackId);

/*** end of spatha  ***/

static int icx72x0_dom_init(void);
static int icx72x0_om_scheduler_init(struct om_sched *p_sched);
static void icx72x0_om_init(void);
static PORT_ID icx72x0_get_port_from_sfp_monitor(int stackId, int sfp_monitor);
static unsigned int icx72x0_get_sfp_monitor_from_port(PORT_ID port);
static int icx72x0_get_num_sfp_monitors(int stackId);

/* Minions */
static int icx7150_dom_init(void);
static int icx7150_om_scheduler_init(struct om_sched *p_sched);
static PORT_ID icx7150_get_port_from_sfp_monitor(int stackId, int sfp_monitor);
static unsigned int icx7150_get_sfp_monitor_from_port(PORT_ID port);
static int icx7150_get_num_sfp_monitors(int stackId);
static int sfpp_sfp_get_measure(PORT_ID port, qsfpp_port_optic_t *port_optic);
static int stack_member_sfpp_sfp_get_measure(PORT_ID port, pp_stack_port_optic_ipc_header *mem_port_optic);

/* common om scheduler init */
static int
om_scheduler_init()
{
  memset(&service, 0, sizeof(struct om_sched));
  strcpy(service.name, "service");

  memset(&monitor, 0, sizeof(struct om_sched));
  strcpy(monitor.name, "monitor");
  return 0;
}

/* get total number of ports to serve per cycle for a slot */
static int
om_scheduler_get_slot_ports_per_cycle(struct om_sched *p_sched, int slot)
{
  return p_sched->slots[slot].num_ports_per_cycle;
}

/* get a slot to serve */
static int
om_scheduler_get_serv_slot(struct om_sched *p_sched)
{
  int slot = p_sched->serv_slot;
  int num_slots = p_sched->num_slots;

  /* serach for an enabled slot */
  while (!p_sched->slots[slot].enable && --num_slots) slot++;

  p_sched->serv_slot = slot;
  return p_sched->serv_slot;
}

/* get a port to serve */
static int
om_scheduler_get_serv_port(struct om_sched *p_sched)
{
  struct om_sched_slot *p_slot = &p_sched->slots[p_sched->serv_slot];

  return p_slot->serv_port;
}

static void
om_scheduler_update(struct om_sched *p_sched)
{
  struct om_sched_slot *p_slot = &p_sched->slots[p_sched->serv_slot];

  p_slot->num_port_served++;
  p_slot->serv_port++;  /* port to serve next */
  if (p_slot->serv_port != p_slot->num_ports)
    return;

  else /* done with the current slot, move on to with next */
  {
     int next_slot = p_sched->serv_slot + 1;

     /* reset om_scheduler.serv_slot */
     p_slot->serv_port = 0;

     /* update stats */
     p_sched->num_slot_served++;

     p_sched->serv_slot = (next_slot == p_sched->num_slots)? 0 : next_slot;
  }
}

static void
om_scheduler_show(struct om_sched *p_sched)
{
  int s;
  uprintf("%s: total slots: %d, next slot: %d, \n", 
                 p_sched->name, p_sched->num_slots, p_sched->serv_slot);

  for (s = 0; s < p_sched->num_slots; s++)
  {
    uprintf("slot: %d, total ports: %d, next port: %d\n", 
                  p_sched->slots[s].slot, p_sched->slots[s].num_ports,  
                  p_sched->slots[s].serv_port);
  }
  uprintf("\n");
}


int 
get_port_sfp_monitor_index (PORT_ID lport)
{
  return get_sfp_monitor_from_port(lport);
}

static int
sfp_get_vendor_name (PORT_ID port, UINT8 *buf)
{
  int stackId = PORT_TO_STACK_ID(port);
  PORT_ID local_port = PORT_TO_STACK_PORT(port);

  if (stackId != MY_BOOTUP_STACK_ID) 
     return LINK_OK;

  return sfp_media_read(port, SFP_EEPROM_ADDR, SFP_VENDOR_NAME, SFP_VENDOR_NAME_LEN, buf);
}

static int
is_qsfp_optic_dom_supported (PORT_ID port)
{

	if ((SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_40GBASE_SR4) ||
		(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_4X10G_SR4) ||
		(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_40GBASE_ESR4) ||
		(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_40GBASE_LM4) ||
		(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_40GBASE_ER4) ||
		(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_40GBASE_LR4))
		return 1;
	else
		return 0;
}

/*
   SFP/QSFP temperature reading
 */
int
sfp_get_temperature (PORT_ID port, signed short *temp)
{
	int ret = LINK_OK, bytes = 0;
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);
	int stackId =  PORT_TO_STACK_ID(port);

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	if (is_qsfp_optic_dom_supported(port)) 
	{
		int bytes = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_TEMPERATURE, 
							QSFP_TEMPERATURE_LEN, (UINT8 *)temp);
		if (bytes != QSFP_TEMPERATURE_LEN) 
		{
			ret = LINK_ERROR; 
			*temp = 0xffff;
		}
	}
	else
	{
		if(is_qsfp_optictype_on_x40g(port))
			return ret;

		bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1,
							SFP_TEMPERATURE, SFP_TEMPERATURE_LEN,(UINT8 *)temp);
		if (bytes != SFP_TEMPERATURE_LEN) 
		{
			ret = LINK_ERROR; 
			*temp = 0xffff;
		}
	}

	OPTMON_DTRACE (("%s: port %p, temp 0x%x\n", __FUNCTION__, port, *temp));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  %s: port %p, temp 0x%x\n", __FUNCTION__, port, *temp);
	
	return ret;
}

/*
   SFP/QSFP supply voltage reading
 */
int
sfp_get_voltage (PORT_ID port, UINT16 *voltage)
{
	int ret = LINK_OK, bytes = 0;
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);
	int stackId =  PORT_TO_STACK_ID(port);

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	if(is_qsfp_optictype_on_x40g(port))
		return ret;

	bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1,
						SFP_VOLTAGE, SFP_VOLTAGE_LEN,(UINT8 *)voltage);
	if (bytes != SFP_VOLTAGE_LEN) 
	{
		ret = LINK_ERROR; 
		*voltage = 0xffff;
	}

	OPTMON_DTRACE (("%s: port %p, voltage 0x%x\n", __FUNCTION__, port, *voltage));
	return ret;
}

/*
   SFP/QSFP Rx power
 */
static int
sfp_get_rx_power (PORT_ID port, UINT16 *power)
{
	int ret = LINK_OK, bytes = 0;
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);
	int stackId =  PORT_TO_STACK_ID(port);

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	if (is_qsfp_optic_dom_supported(port)) 
	{
		bytes = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_RX1_POWER, 
								QSFP_RX1_POWER_LEN, (UINT8 *)power);
		if (bytes != QSFP_RX1_POWER_LEN)
		{
			ret = LINK_ERROR; 
			*power = 0xffff;
		}
	}
	else
	{
		if(is_qsfp_optictype_on_x40g(port))
			return ret;

		bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1,
									SFP_RX_POWER, SFP_RX_POWER_LEN, (UINT8 *)power);
		if (bytes != SFP_RX_POWER_LEN) 
		{
			ret = LINK_ERROR; 
			*power = 0xffff;
		}
	}

	OPTMON_DTRACE (("%s: port %p, power 0x%x\n", __FUNCTION__, port, *power));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  %s: port %p, power 0x%x\n", __FUNCTION__, port, *power);
	
	return ret;
}

/* read qsfp rx_power for channel 2--4 */
int 
qsfp_get_rx2to4_power(PORT_ID port, unsigned short *power)
{
	int ret = LINK_OK;
	int stackId =  PORT_TO_STACK_ID(port);
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);
	int bytes;
	int bytes_to_read = QSFP_RX2_POWER_LEN + QSFP_RX3_POWER_LEN + QSFP_RX3_POWER_LEN;

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	bytes = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_RX2_POWER, 
							bytes_to_read, (UINT8 *)power);
	if (bytes != bytes_to_read)
	{
		ret = LINK_ERROR; 
		*power = 0xffff;
	}
	OPTMON_DTRACE (("%s: port %p, power2 0x%x  power3 0x%x  power4 0x%x\n", 
						__FUNCTION__, port, power[0],  power[1],  power[2]));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  %s: port %p, power2 0x%x  power3 0x%x  power4 0x%x\n", 
								__FUNCTION__, port, power[0],  power[1],  power[2]);
				

	return ret;
}

/*
   SFP/QSFP Tx power
 */
int
sfp_get_tx_power (PORT_ID port, UINT16 *power)
{
	int ret = LINK_OK, bytes = 0;
	int stackId =  PORT_TO_STACK_ID(port);
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	*power = 0xffff;

	if (is_qsfp_optic_dom_supported(port)) 
	{
		ret = LINK_OK; /* QSFP does not provide tx power */
	}	
	else
	{
		if(is_qsfp_optictype_on_x40g(port))
			return ret;

		bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1,
							SFP_TX_POWER, SFP_TX_POWER_LEN, (UINT8 *)power);
		if (bytes != SFP_TX_POWER_LEN)
		{
			ret = LINK_ERROR; 
		}
	}

	OPTMON_DTRACE (("%s port %p power 0x%x \n", __FUNCTION__, port, *power));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s port %p power 0x%x \n", __FUNCTION__, port, *power);
	
	return ret;
}

/*
   SFP/QSFP Tx Bias Current
 */
static int
sfp_get_tx_bias_current (PORT_ID port , UINT16 * bias)
{
	int ret = LINK_OK, bytes = 0;
	int stackId = PORT_TO_STACK_ID(port);
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	*bias = 0xffff;
	if (is_qsfp_optic_dom_supported(port)) 
	{
		bytes = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_TX1_BIAS_CURRENT, 
								QSFP_TX1_BIAS_CURRENT_LEN, (UINT8 *)bias);
		if (bytes != QSFP_TX1_BIAS_CURRENT_LEN)
		{
			ret = LINK_ERROR; 
		}
	}
	else 
	{
		if(is_qsfp_optictype_on_x40g(port))
			return ret;

		bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1,
							SFP_TX_BIAS_CURRENT, SFP_TX_BIAS_CURRENT_LEN,(UINT8 *)bias);
		if (bytes != SFP_TX_BIAS_CURRENT_LEN)
		{
			ret = LINK_ERROR; 
		}
	}

	OPTMON_DTRACE (("%s: port %p, bias 0x%x\n", __FUNCTION__, port, *bias));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s: port %p, bias 0x%x\n", __FUNCTION__, port, *bias);
	return ret;
}

/* read qsfp bias current for channel 2--4 */
int 
qsfp_get_tx2to4_bias(PORT_ID port, unsigned short *bias)
{
	int ret = LINK_OK;
	int stackId =  PORT_TO_STACK_ID(port);
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);
	int bytes;
	int bytes_to_read = QSFP_TX2_BIAS_CURRENT_LEN + 
						QSFP_TX3_BIAS_CURRENT_LEN + 
						QSFP_TX4_BIAS_CURRENT_LEN;

	if (!IS_PORT_DB_VALID(port))
		return LINK_ERROR;
	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	bytes = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_TX2_BIAS_CURRENT, 
							bytes_to_read, (UINT8 *)bias);
	if (bytes != bytes_to_read)
	{
		ret = LINK_ERROR; 
		*bias = 0xffff;
	}
	OPTMON_DTRACE (("%s: port %p, bias2 0x%x  bias3 0x%x  bias4 0x%x\n", 
						__FUNCTION__, port, bias[0],  bias[1],  bias[2]));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s: port %p, bias2 0x%x  bias3 0x%x  bias4 0x%x\n", 
						__FUNCTION__, port, bias[0],  bias[1],  bias[2]);
	
	return ret;
}

int 
qsfp_get_volt(PORT_ID port, unsigned short *volt)
{
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);
	int stackId =  PORT_TO_STACK_ID(port);
	int bytes;
	int ret = LINK_OK;

	if (!IS_PORT_DB_VALID(port))
		return LINK_OK;

	if (stackId != MY_BOOTUP_STACK_ID)
		return LINK_OK;

	bytes = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_VOLTAGE, 
							QSFP_VOLTAGE_LEN, (UINT8 *)volt);
	if (bytes != QSFP_VOLTAGE_LEN)
	{
		ret = LINK_ERROR; 
		*volt = 0xffff;
	}

	OPTMON_DTRACE (("%s: port %p, volt 0x%x\n", __FUNCTION__, port, *volt));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s: port %p, volt 0x%x\n", __FUNCTION__, port, *volt);
	
	return ret;
}


/*
   SFP Monitor thresholds
 */
static void
sfp_get_thresholds (PORT_ID lport, void *thresholds)
{
  int i, index;
  int found = 0;
  int stackId = PORT_TO_STACK_ID(lport);

  index = get_port_sfp_monitor_index(lport);
  if (index == -1) return;

  memset (thresholds, 0xff, sizeof (SFP_MONITOR_THRESHOLDS));
  memcpy (thresholds, 
          (void *) &sfp_monitor[stackId][index].thresholds, 
           sizeof (SFP_MONITOR_THRESHOLDS));
}


static int 
optical_monitor_data_ready(PORT_ID lport)
{
  int stackId = PORT_TO_STACK_ID(lport);
  int index = get_port_sfp_monitor_index(lport);

  if (index == -1) 
  {
    OPTMON_DTRACE (("hal_sfp_monitor_thresholds_display: not found\n"));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: hal_sfp_monitor_thresholds_display: not found\n");
    return 0;
  }

  /* DOM enabled, but it might take minutes when sfp_monitor gets initialized */
  if ((SPTR_PORT_DB(lport)->port_config.optical_monitor_interval > 0) &&
      (sfp_monitor[stackId][index].Optical_monitor_use != OPTICS_IN_MONITOR))
  {
    uprintf ("Optical monitoring is in progress, please try later\n");
    return 0;
  }

  return 1;
}


void
hal_sfp_monitor_thresholds_display (PORT_ID lport)
{
  int i, index, temp, precision;
  UINT16 *ptr;
  UINT32 val;
  int found = 0;
  int stackId = PORT_TO_STACK_ID(lport);

#if 0
	if(SPTR_PORT_DB(lport)->port_mtype == X10GIG_FIBER_PORT) { // It is 10G 
		if(!is_xfp_capable_of_optical_monitoring(lport)) { // non-Brocade XFP, SFPP
			if(!dom_non_brocade_on_off_set) // not set for non-Brocade optics
				return;
		}
	} else if(SPTR_PORT_DB(lport)->port_mtype == GIG_FIBER_PORT) { // it is 1G fiber
		if(is_sfp_capable_of_optical_monitoring(lport) != SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE) {
			if(!dom_non_brocade_on_off_set) // if 1G fiber other vendors, and not set it
				return;
		}
	}
#endif

  if(!(dm_debug_mask&DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
    if(!IS_PORT_UP(lport) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(lport))->port_config.dom_down_port_set)  // If Brocade optics but no debugging 
      return;

  index = get_port_sfp_monitor_index(lport);
  if (index == -1) 
  {
    debug_uprintf ("%s:%d: can not find sfp_monitor for port %p\n", 
                     __FILE__, __LINE__, lport);
    return;
  }

  ptr = (UINT16 *) & sfp_monitor[stackId][index].thresholds.temp_high_alarm;

  // TR 573328, 574767,571293 When Active issued the threshold shown, should let it pass through called by
  // cu_show_stack_port_sfp_optic_ipc_callback_from_active_unit()
//  if(!STACK_AM_I_SLAVE) 
  if(stackId == MY_BOOTUP_STACK_ID) 
  if(!optical_monitor_data_ready(lport)) 
    return;

  if (stackId != MY_BOOTUP_STACK_ID )
    g_uprintf_dest = g_uprintf_dest_save;
	
  if ((SHOW_BUF = sv_buf_alloc()) != NULL)
    redirect_output_to_display_buf();


  if(is_qsfp_optictype_on_x40g(lport))
      uprintf ("Port %p qsfp monitor thresholds:\n", lport);
  else
      uprintf ("Port %p sfp monitor thresholds:\n", lport);

  for (i = 0; i < 20; ++i)
  {
    {
      uprintf ("%s %4x", sfp_monitor_threshold_string[i], *ptr);
      if (i < 4)
      {
        temp = (int) *ptr >> 8;
        if (*ptr & 0x8000)
        {
          temp |= 0xffffff00;
        }

        precision = (int) (((*ptr & 0xff) * 10000) / 256);
        uprintf ("    %3d.%04d C ", temp, precision);
      }
      else if (i >= 4 && i < 8)
      {
        uprintf ("      %1d.%04d Volts", *ptr / 10000, *ptr % 10000);
      }
      else if (i >= 8 && i < 12)
      {
        uprintf ("    %3d.%04d mA", *ptr / 500, (*ptr * 2) % 1000);
      }
      else if (i >= 12 && i < 20)
      {
        if (mw2dbm (*ptr, &val))
        {
          uprintf ("   -%03d.%04d dBm", val / 10000, val % 10000);
        }
        else
        {
          uprintf ("    %03d.%04d dBm", val / 10000, val % 10000);
        }
      }
      uprintf ("\n");
    }
    ptr++;
  }

  if (SHOW_BUF != NULL)
    sv_buf_display();
	
  if (stackId != MY_BOOTUP_STACK_ID)
    print_prompt(&cdbs[g_uprintf_dest]);
}

static void
sfp_monitor_set_interval (int index, PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);

  sfp_monitor[stackId][index].interval = 
          (SPTR_PORT_DB (port))->port_config.optical_monitor_interval * 60;
}

static void
sfp_monitor_display_flag (int index, PORT_ID lport, UINT32 curr_flag0, UINT32 curr_flag1)
{
  int i, k;
  char *bp = cu_line_buf;
  int flag_alarm = 0, flag_warn = 0;

  OPTMON_DTRACE (("sfp_monitor_display_flag()- port=%p, index=%d curr_flag0=%x curr_flag1=%x\n",
                  lport, index, curr_flag0, curr_flag1));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: sfp_monitor_display_flag()- port=%p, index=%d curr_flag0=%x curr_flag1=%x\n",
                  lport, index, curr_flag0, curr_flag1);

  /* Alarm */
  for (i = 0; i < 10; ++i)
  {
    if (curr_flag0 & 1 << (31 - i))
    {
      k = ksprintf (bp, "    %s, port %p\n", sfp_monitor_flag_string[i], lport);
      bp += k;
      flag_alarm = 1;
    }
  }

  if (flag_alarm)
  {
    OPTMON_DTRACE (("%s", cu_line_buf));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s", cu_line_buf);
	 
    if (IS_PORT_UP (lport) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set 
       || (SPTR_PORT_DB(lport))->port_config.dom_down_port_set)
    {
      send_port_optical_msg_type (cu_line_buf, OPTICAL_MONITORING_ALARM_MSG_TYPE);
    }
  }

  /* Warning */
  bp = cu_line_buf;
  for (i = 0; i < 10; ++i)
  {
    if (curr_flag1 & 1 << (31 - i))
    {
      k = ksprintf (bp, "    %s, port %p\n", sfp_monitor_flag_string[32 + i], lport);
      bp += k;
      flag_warn = 1;
    }
  }

  if (flag_warn)
  {
    OPTMON_DTRACE (("%s", cu_line_buf));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s", cu_line_buf);
	
    if (IS_PORT_UP (lport) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set 
      || (SPTR_PORT_DB(lport))->port_config.dom_down_port_set)
    {
      send_port_optical_msg_type (cu_line_buf, OPTICAL_MONITORING_WARN_MSG_TYPE);
    }
  }

}

static int 
read_sfp_interrupt_flags(PORT_ID lport,unsigned char * buf)
{
  if (is_qsfp_optic_dom_supported(lport)) 
  {
    return qsfp_read_monitoring_flags(lport, buf);
  }
  else
  {
	return sfp_read_monitoring_flags(lport, buf);
  }
  return LINK_OK;
}


static void
sfp_optical_monitoring (PORT_ID lport)
{
  UINT8 buf[64];
  UINT32 curr_flag0, curr_flag1, temp_flag0, temp_flag1;
  int index;
  int found = 0;
  int stackId = PORT_TO_STACK_ID(lport);

  if (stackId != MY_BOOTUP_STACK_ID)
    return;     

  // If this is a LRM fluffy adapter, we check if external LRM optic existed
  if(!IS_SPATHA()) { // Spatha does not support this. Here are base on media_type
    if(SPTR_PORT_DB(lport)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER) {
      if(is_fluffy_ext_lrm_optics_present(lport) != LINK_OK)  // external optic does not exist
        return;
    }
  }

  index = get_port_sfp_monitor_index(lport);
  if (index == -1) 
  {
    debug_uprintf ("%s:%d: can not find sfp_monitor for port %p\n", 
                     __FILE__, __LINE__, lport);
    return;
  }


  OPTMON_DTRACE (("\nsfp_optical_monitoring() - index=%d port=%p interval=%d sfp_monitor_interval=%d\n", 
                  index, lport,
                  SPTR_PORT_DB (lport)->port_config.optical_monitor_interval, 
                  sfp_monitor[stackId][index].interval));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nsfp_optical_monitoring() - index=%d port=%p interval=%d sfp_monitor_interval=%d\n", 
                  index, lport,
                  SPTR_PORT_DB (lport)->port_config.optical_monitor_interval, 
                  sfp_monitor[stackId][index].interval);

/*Need to call the send API with om_enabled_port_list[] and perform the operation over DOM process
 * The rest of this function execution will happen when we recieve the message from DOM with updated value and 
 * proceeds further*/


  /* We need to gather the calibration value first for SNMP */ 
      if(SPTR_PORT_DB(lport)->port_oper_info.snmp_dom_set_flag==0) {
         sw_cu_get_calibration_measure(lport);
           SPTR_PORT_DB(lport)->port_oper_info.snmp_dom_set_flag = 1;
    }

  if (check_set_interval (SFP_OPTICS, index, lport) == 0)
  {
    return;
  }

  if(read_sfp_interrupt_flags(lport,buf) != LINK_OK)
    return;	

  temp_flag0 = *(UINT32 *) buf;
  temp_flag1 = *(UINT32 *) (buf + 4);

  if((temp_flag0) || (temp_flag1))
  {
	  /* The SFP tends to generate momentary latches during optic initialization.
	  Before sending syslog messages, read again to confirm if the latches generated are genuine */
	  if(read_sfp_interrupt_flags(lport, buf) != LINK_OK)
		  return;
  
	  curr_flag0 = *(UINT32 *)buf;
	  curr_flag1 = *(UINT32 *) (buf + 4);
  
	  /* Allow only those latches which were set both the times */
	  
	  curr_flag0 &= temp_flag0;	
	  curr_flag1 &= temp_flag1;
  }
  else 
  {
    curr_flag0 = temp_flag0;
    curr_flag1 = temp_flag1;
  }

  curr_flag0 &= sfp_monitor[stackId][index].enable0;
  curr_flag1 &= sfp_monitor[stackId][index].enable1;

  sfp_monitor_display_flag (index, lport, curr_flag0, curr_flag1);

  sfp_monitor[stackId][index].flag0 = curr_flag0;
  sfp_monitor[stackId][index].flag1 = curr_flag1;

  OPTMON_DTRACE (("\nsfp_optical_monitoring() - sfp_monitor[%d].flag0 =%d, flag1= %d\n", 
                     index, curr_flag0, curr_flag1));

   fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nsfp_optical_monitoring() - sfp_monitor[%d].flag0 =%d, flag1= %d\n", 
                     index, curr_flag0, curr_flag1);
   
}

 

/* 
 * Below source code is for 4-port scheduling of every interval to prevent from high CPU.
 * Customers had complained high CPUs during SX, ICX6610(max 24 Fiber ports) and Reaper (max 68 ports).
 * ICX7750 has 48 10G, 12 QSFP fiber ports. Each port requires 6+ I/O access and can cause high CPU when it overs 6+ port at once.
 * This code works for ICX7750 standalone, breakout and 802.1BR. It checks every port until last port and goes back to the 1st port of 1st slot
 * This is only for physical ports for CB since PE might be non-Brocade platforms and may not support DOM features.
 * We only access 40G QSFP once even it has been set to four 10G ports by breakout feature. This prevent from accessing additional I/O.
 * icx7750_slot_serv and icx7750_sl are set during DOM init for the first slot of Stack.
 */
void icx7750_optical_monitoring_service( void )
{
	int port_i,port_run;
	int OMSlotNum=0;
	PORT_ID sfpp_port, last_port_s = 0xff;
	int num_of_port=0;
	extern UINT32 gi_board_type;

	for(OMSlotNum = icx7750_slot_serv; OMSlotNum < MAXSLOT_ICX7750_PER_STACK; OMSlotNum++)
	{
		if((icx7750_slot_serv==ICX7750_SLOT2) && (icx7750_total_port_count_serv==ICX7750_PORT_SLOT1)) 
		{
			icx7750_total_port_count_serv = DOM_PORT_MONITOR_QSFP_PER_TIME; // DOM_PORT_MONITOR_PER_TIME; for slot 2
			return; // finish slot 0
		} else if((icx7750_slot_serv==ICX7750_SLOT3) && (last_port_s==ICX7750_LASTPORT_SLOT2))
			return; // finish slot 1

		sfpp_port = MAKE_PORTID(OMSlotNum, port_count_icx7750_serv);
		switch(OMSlotNum%4) {
			case 0: num_of_port = ICX7750_PORT_SLOT1; port_run = 1; // for 48F and 48C will skip
				if (gi_board_type == SW_BOARD_TYPE_20QXG) {
				// breakout port X/1/1-X/1/4 X/1/17-X/1/20 - 8 ports; X/1/5 - X/1/16: 4x12=48 ports. We count physical ports
					num_of_port = MAX_ICX7750_SLOT1_QSFP;
					if((PORT_TO_MODULE_PORT(sfpp_port)>=4)&&(PORT_TO_MODULE_PORT(sfpp_port)<=16)) {
						port_run=4; // every main port only and skip other 3 sub-ports
						// port 1/1/17 is logical port 52, port 1/1/17-1/1/20 is per port increment
						if(PORT_TO_MODULE_PORT(sfpp_port)==16) port_run=1;  
						// get the right sw port by stack id# sfpp_port=(sfpp_port-3)*4;
						sfpp_port=(MY_BOOTUP_STACK_ID-1)*SW_PORTS_PER_UNIT+((sfpp_port%SW_PORTS_PER_UNIT)-3)*SW_NUM_MODULES_PER_UNIT;
					}
				}
				break;
			case 1: num_of_port = ICX7750_QSFP_SLOT2; port_run = 4; break; // breakout ports define 4x10G
			default:
			case 2: num_of_port = ICX7750_QSFP_SLOT3; port_run = 4; break; // breakout ports define 4x10G
		}

		for(port_i = port_count_icx7750_serv; port_i<num_of_port; port_i++, sfpp_port+=port_run)
		{
			if(port_i==icx7750_total_port_count_serv) { // manage 4 ports only, 1st port is 0 port
				if(icx7750_slot_serv != ICX7750_SLOT2) {
					icx7750_total_port_count_serv += DOM_PORT_MONITOR_PER_TIME;
					port_count_icx7750_serv = port_i; 
				}
				return;
			}
			if(port_i == (num_of_port-1)) {
				port_count_icx7750_serv = 0;
				if(icx7750_slot_serv == ICX7750_SLOT1) {
					icx7750_slot_serv = ICX7750_SLOT2;
					icx7750_total_port_count_serv = ICX7750_PORT_SLOT1; 
				} else if(icx7750_slot_serv == ICX7750_SLOT2) {
					icx7750_slot_serv = ICX7750_SLOT3;
					last_port_s = sfpp_port%SW_PORTS_PER_UNIT;
					icx7750_total_port_count_serv = DOM_PORT_MONITOR_QSFP_PER_TIME; 
				} else if(icx7750_slot_serv == ICX7750_SLOT3) {
					icx7750_slot_serv = ICX7750_SLOT1;
					icx7750_total_port_count_serv = DOM_PORT_MONITOR_PER_TIME; 
				}
			}
			if(dm_debug_mask & DM_OPTICAL_MONITOR_QA)
				uprintf("Serve DOM mod_port=%d gport=%d %p num_of_port=%d BootID=%d\n",port_i,sfpp_port,sfpp_port, num_of_port,MY_BOOTUP_STACK_ID);

			if(!IS_PORT_DB_VALID(sfpp_port))
				continue;

			if(SPTR_PORT_DB(sfpp_port)->port_config.optical_monitor_interval == 0)
				continue;

			if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.mibdom))
			  if (!IS_PORT_UP(sfpp_port) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(sfpp_port))->port_config.dom_down_port_set) continue;

			if((SPTR_PORT_DB(sfpp_port)->port_mtype == X10GIG_FIBER_PORT)||
				(SPTR_PORT_DB(sfpp_port)->port_mtype == GIG_FIBER_PORT) ||
				(SPTR_PORT_DB(sfpp_port)->port_mtype == X40G_STACK_PORT))
			{
				if(!is_sidewinder_optic_present(sfpp_port))
				{
					if (debugGlobal.system.optics)
						uprintf("icx7750 port %p SFP+/QSFP+ not present.\n", sfpp_port);
					continue;
				}

				if((SPTR_PORT_DB(sfpp_port)->port_oper_info.media_type == LINK_MEDIA_INVALID) ||
					(SPTR_PORT_DB(sfpp_port)->port_oper_info.media_type == MEDIA_TYPE_1000BASE_EMPTY))
					icx7750_media_assign(sfpp_port);

				if(is_qsfp_optictype_on_x40g(sfpp_port) && !is_qsfp_optic_dom_supported(sfpp_port))
					continue;

				if((SPTR_PORT_DB(sfpp_port)->port_oper_info.media_type != LINK_MEDIA_GIG_COPPER) &&
					(SPTR_PORT_DB (sfpp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_COPPER )&&
					(SPTR_PORT_DB(sfpp_port)->port_oper_info.media_type != LINK_MEDIA_INVALID))
					sfp_optical_monitoring(sfpp_port);
			}
		}
	}
	return;
}

void icx7750_optical_monitor( void )
{
	int iom_11, OMSlotNum, port_run;
	PORT_ID lport=0, DM_lport=0,last_port=0xff;
	PORT_DB *port_ptr;
	int total_num_of_port=0;
	extern UINT32 gi_board_type;

	for(OMSlotNum = icx7750_sl; OMSlotNum < MAXSLOT_ICX7750_PER_STACK; OMSlotNum++)
	{
		if((icx7750_sl==ICX7750_SLOT2) && (icx7750_total_port_cnt==ICX7750_PORT_SLOT1)) {
			icx7750_total_port_cnt = DOM_PORT_MONITOR_QSFP_PER_TIME; // DOM_PORT_MONITOR_PER_TIME; 
			return; // finish slot 0
		} else if((icx7750_sl==ICX7750_SLOT3) && (last_port==ICX7750_LASTPORT_SLOT2))
			return; // finish slot 1

		DM_lport = MAKE_PORTID(OMSlotNum, port_count_icx7750);
		switch(OMSlotNum%4) {
			case 0: total_num_of_port = ICX7750_PORT_SLOT1; port_run = 1; // for 48F SFP/SFPP and 48C will skip
				if (gi_board_type == SW_BOARD_TYPE_20QXG) {
					// breakout port X/1/1-X/1/4 X/1/17-X/1/20 - 8 ports;  X/1/5 - X/1/16: 4x12=48 ports
					total_num_of_port = MAX_ICX7750_SLOT1_QSFP; 
					if((PORT_TO_MODULE_PORT(DM_lport)>=4) && (PORT_TO_MODULE_PORT(DM_lport)<=16)) {
						port_run=4; // every main port only and skip other 3 sub-ports
						// port 1/1/17 is logical port 52, port 1/1/17-1/1/20 is per port increment
						if(PORT_TO_MODULE_PORT(DM_lport)==16) port_run=1;
						// get the right sw port by stack id#
						DM_lport=(MY_BOOTUP_STACK_ID-1)*SW_PORTS_PER_UNIT+((DM_lport%SW_PORTS_PER_UNIT)-3)*SW_NUM_MODULES_PER_UNIT;
					}
				}
				break;
			case 1: total_num_of_port = ICX7750_QSFP_SLOT2; port_run = 4; break;
			default:
			case 2: total_num_of_port = ICX7750_QSFP_SLOT3; port_run = 4; break;
		}

		for(iom_11=port_count_icx7750; iom_11<total_num_of_port; iom_11++, DM_lport+=port_run)
		{
			if(iom_11==icx7750_total_port_cnt) {
				if(icx7750_sl != ICX7750_SLOT2) {
					icx7750_total_port_cnt += DOM_PORT_MONITOR_PER_TIME;
					port_count_icx7750 = iom_11;
				}
				return;
			}
			if(iom_11 == (total_num_of_port-1)) {
				port_count_icx7750 = 0;
				if(icx7750_sl == ICX7750_SLOT1) {
					icx7750_sl = ICX7750_SLOT2;
					icx7750_total_port_cnt = ICX7750_PORT_SLOT1; 
				} else if(icx7750_sl == ICX7750_SLOT2) {
					icx7750_sl = ICX7750_SLOT3;
					last_port = DM_lport%SW_PORTS_PER_UNIT;
					icx7750_total_port_cnt = DOM_PORT_MONITOR_QSFP_PER_TIME; 
				} else if(icx7750_sl == ICX7750_SLOT3) {
					icx7750_sl = ICX7750_SLOT1;
					icx7750_total_port_cnt = DOM_PORT_MONITOR_PER_TIME; 
				}
			}
			if(dm_debug_mask & DM_OPTICAL_MONITOR_QA)
				uprintf("DOM mod_port=%d gport=%d slot=%d num_of_ports=%d\n",iom_11,DM_lport,OMSlotNum,total_num_of_port);

			if(!IS_PORT_DB_VALID(DM_lport))
				continue;

			port_ptr = SPTR_PORT_DB(DM_lport);
			if(port_ptr->port_config.optical_monitor_interval == 0)
				continue;

			if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
			  if (!IS_PORT_UP(DM_lport) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(DM_lport))->port_config.dom_down_port_set)
				continue;

			if(!is_sidewinder_optic_present(DM_lport))
			{
				op_zap_per_port_data(DM_lport);
				continue;
			}

			if((port_ptr->port_mtype == GIG_FIBER_PORT && port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_UNKNOWN)
				|| (port_ptr->port_mtype == X10GIG_FIBER_PORT && port_ptr->port_oper_info.media_type == LINK_MEDIA_INVALID))
				continue;

			if (port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_TX || 
				port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_CX ||
				port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHB || 
				port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CX4 ||
				port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_COPPER || 
				port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_ACTIVE_COPPER || 
				port_ptr->port_oper_info.media_type == LINK_MEDIA_1G_TWINAX ||
				port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CABLE ||
				port_ptr->port_oper_info.media_type == LINK_MEDIA_1GBASE_ACTIVE_CABLE)
			{
			  op_zap_per_port_data(DM_lport);
			  port_ptr->port_config.optical_monitor_interval = 0;
			  continue;
			}

			if((port_ptr->port_oper_info.media_type != MEDIA_TYPE_1000BASE_EMPTY))
			{
				if(port_ptr->port_mtype == GIG_FIBER_PORT ||
					port_ptr->port_mtype == X10GIG_FIBER_PORT ||
					is_sfp_sfpp_optictype_on_x40g(DM_lport) ||
					(is_qsfp_optictype_on_x40g(DM_lport) && is_qsfp_optic_dom_supported(DM_lport)))
					sfp_monitor_data_init(DM_lport);
			}
		}
	}
	return;
}

void 
hal_sfp_optical_monitoring_service (void)
{
  int OMSlotNum = 0;
  PORT_DB *port_ptr;
  int stackId = 1, stack_slot =0;
  int p, max_port_serv;
  
  if (IS_SIDEWINDER()) {
      icx7750_optical_monitoring_service();
      return;
  }
  
  for (OMSlotNum = 0; OMSlotNum < g_hw_info.max_mod; OMSlotNum++)
  {
    if(is_module_exist(OMSlotNum))
    {
       stackId = MODULE_TO_STACK_ID(OMSlotNum);
       stack_slot = MODULE_TO_STACK_MODULE(OMSlotNum);

       if (stackId != MY_BOOTUP_STACK_ID)
         continue;

       if (!is_module_exist(stack_slot)) continue;

       /* is this slot scheduled to serve? */
       if (om_scheduler_get_serv_slot(&service) != stack_slot) continue;


       if (debugGlobal.system.optics)
       {
         om_scheduler_show(&service);
         uprintf("%s: serve slot %d\n", __FUNCTION__, stack_slot);
       }

       max_port_serv = om_scheduler_get_slot_ports_per_cycle(&service, stack_slot);

       for (p = 0; p < max_port_serv; p++)
       {
         int module_port = om_scheduler_get_serv_port(&service);
         PORT_ID sfp_port = MAKE_PORTID(OMSlotNum, module_port);

         if (!IS_PORT_DB_VALID(sfp_port))
           continue;

         om_scheduler_update(&service);

         if (SPTR_PORT_DB(sfp_port)->port_config.optical_monitor_interval == 0)
           continue;

         if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
           if (!IS_PORT_UP(sfp_port) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(sfp_port))->port_config.dom_down_port_set) continue;

         if ((SPTR_PORT_DB(sfp_port)->port_mtype == X10GIG_FIBER_PORT) ||
             (SPTR_PORT_DB(sfp_port)->port_mtype == GIG_FIBER_PORT)    ||
             (SPTR_PORT_DB(sfp_port)->port_mtype == X40G_STACK_PORT))
         {	
           if (!hal_is_xfp_sfp_present(sfp_port))
           {
             if (debugGlobal.system.optics)
               uprintf("Port %p SFP+/QSFP+ not present.\n", sfp_port);
             continue;
           }

           if ((SPTR_PORT_DB(sfp_port)->port_oper_info.media_type == LINK_MEDIA_INVALID) ||
               (SPTR_PORT_DB(sfp_port)->port_oper_info.media_type == MEDIA_TYPE_1000BASE_EMPTY))
             pp_link_media_assign(sfp_port);

 
	   if(is_qsfp_optictype_on_x40g(sfp_port) && !is_qsfp_optic_dom_supported(sfp_port))
             continue;

           if ((SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_GIG_COPPER) &&
               (SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_GIG_COPPER_GBIC) &&
               (SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_1G_TWINAX) && 
               (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_COPPER )&&
               (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_ACTIVE_COPPER)&&
               (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_CABLE)&&
               (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_CX4)&&
               (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_1GBASE_ACTIVE_CABLE)&&
                (SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_INVALID))
             sfp_optical_monitoring(sfp_port);
         }
       }
    }
  }
}

struct OM_PORTS om_ports;
struct om_port_data om_data;

void transaction_between_dom_and_FI(int flag)
{
	uprintf("Inside transaction_between_dom_and_FI \r\n");
	int local_port = PORT_TO_LOCAL_PORT(om_data.port);
	PORT_ID port = om_data.port;
	if (is_qsfp_optic_dom_supported(port))
		om_ports.optic = 1;
	else
		om_ports.optic = 2;

	om_ports.local_port = local_port;
	om_ports.stack_id = MY_BOOTUP_STACK_ID;
	if(flag == 1){
		om_enabled_port_list[local_port] = (SPTR_PORT_DB(port))->port_config.optical_monitor_interval;
		om_ports.interval = (SPTR_PORT_DB(port))->port_config.optical_monitor_interval;
	
	}else if(flag == 0){
	
		om_enabled_port_list[local_port] = 0;
		om_ports.interval = 0;
	}
}



void tanto_dom_init(PORT_ID port)
{
	uprintf("Inside tanto_dom_init \r\n");
	PORT_ID local_port = PORT_TO_LOCAL_PORT(port);	
	
	if (tanto_optical_monitoring(port) == 1 && tanto_sfp_optical_monitoring_service(port) == 1){

		om_enabled_port_list[local_port] = (SPTR_PORT_DB(port))->port_config.optical_monitor_interval;
		om_ports.local_port = PORT_TO_LOCAL_PORT(port);
		om_ports.interval = (SPTR_PORT_DB(port))->port_config.optical_monitor_interval;
		om_ports.stack_id = MY_BOOTUP_STACK_ID;
		if (is_qsfp_optic_dom_supported(port))
			om_ports.optic = 1;
		else
			om_ports.optic = 2;

		extern void send_notification_to_dom(struct OM_PORTS*);
		send_notification_to_dom(&om_ports);

	}else{
		om_enabled_port_list[local_port] = 0;
	}

	
}



int tanto_sfp_optical_monitoring_service(PORT_ID sfp_port)
{
	int slot,OMSlotNum,module_port;
  	OMSlotNum = module_port = 0;
  	PORT_DB *port_ptr;
  	int stackId = 1, stack_slot =0;
  

    if (!IS_PORT_DB_VALID(sfp_port))
    	return 0;

        // om_scheduler_update(&service);

   	if (SPTR_PORT_DB(sfp_port)->port_config.optical_monitor_interval == 0)
    	return 0;

    if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
    	if (!IS_PORT_UP(sfp_port) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(sfp_port))->port_config.dom_down_port_set) 
			return 0;

    if ((SPTR_PORT_DB(sfp_port)->port_mtype == X10GIG_FIBER_PORT) ||
    	(SPTR_PORT_DB(sfp_port)->port_mtype == GIG_FIBER_PORT)    ||
        (SPTR_PORT_DB(sfp_port)->port_mtype == X40G_STACK_PORT))
    {	
    	if (!hal_is_xfp_sfp_present(sfp_port))
        {
        	if (debugGlobal.system.optics)
            	uprintf("Port %p SFP+/QSFP+ not present.\n", sfp_port);
            return 0;
        }

        if ((SPTR_PORT_DB(sfp_port)->port_oper_info.media_type == LINK_MEDIA_INVALID) ||
        	(SPTR_PORT_DB(sfp_port)->port_oper_info.media_type == MEDIA_TYPE_1000BASE_EMPTY))
        	pp_link_media_assign(sfp_port);

 
	   	if(is_qsfp_optictype_on_x40g(sfp_port) && !is_qsfp_optic_dom_supported(sfp_port))
             return 0;

        if ((SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_GIG_COPPER) &&
        	(SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_GIG_COPPER_GBIC) &&
            (SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_1G_TWINAX) && 
            (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_COPPER )&&
            (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_ACTIVE_COPPER)&&
            (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_CABLE)&&
            (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_10GBASE_CX4)&&
            (SPTR_PORT_DB (sfp_port)->port_oper_info.media_type != LINK_MEDIA_1GBASE_ACTIVE_CABLE)&&
            (SPTR_PORT_DB(sfp_port)->port_oper_info.media_type != LINK_MEDIA_INVALID))
             		return 1;
	}
	return 0;
}


int tanto_optical_monitoring (PORT_ID DM_lport)
{
	int iom, OMSlotNum;
  	PORT_DB *port_ptr;
  	int stackId = 1, stack_slot =0;	


  	if (!IS_PORT_DB_VALID (DM_lport))
    	return 0;

    port_ptr = SPTR_PORT_DB (DM_lport);
    if (port_ptr->port_config.optical_monitor_interval == 0)
        return 0;

    if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
    	if (!IS_PORT_UP(DM_lport) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(DM_lport))->port_config.dom_down_port_set)
        	return 0;

    if (!hal_is_xfp_sfp_present(DM_lport))
    {
        op_zap_per_port_data(DM_lport);
        return 0;
    }

    if (port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CX4 ||
		SPTR_PORT_DB(DM_lport)->port_oper_info.media_type == LINK_MEDIA_10GBASE_COPPER ||
		SPTR_PORT_DB(DM_lport)->port_oper_info.media_type == LINK_MEDIA_1GBASE_ACTIVE_CABLE)
    	return 0;

	if (SPTR_PORT_DB(DM_lport)->port_mtype == X10GIG_FIBER_PORT) 
	{
		if(!dom_non_brocade_on_off_set && !(SPTR_PORT_DB(DM_lport))->port_config.dom_non_brocade_set) // if non-brocade support is not set, check; down-ports check in hal_optical_monitor()
		if(!is_sfpp_capable_of_optical_monitoring(DM_lport))
		{
			uprintf("port %p is not capable of digital optical monitoring.\n", DM_lport); 
			return 0;
		}
	} 

    if ((port_ptr->port_mtype == GIG_FIBER_PORT && 
    	port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_UNKNOWN) ||
        (port_ptr->port_mtype == X10GIG_FIBER_PORT && 
        port_ptr->port_oper_info.media_type == LINK_MEDIA_INVALID))
        return 0;

#if 0
      if(port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHA)
        if(port_ptr->port_oper_info.dom_capable != TRUE)
          continue;
#endif

    if (port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_TX || 
        port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_CX ||
        port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHB || 
        port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_COPPER || 
        port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_ACTIVE_COPPER || 
        port_ptr->port_oper_info.media_type == LINK_MEDIA_1G_TWINAX ||
        port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CABLE ||
        port_ptr->port_oper_info.media_type == LINK_MEDIA_1GBASE_ACTIVE_CABLE)
    {
        op_zap_per_port_data(DM_lport);
        port_ptr->port_config.optical_monitor_interval = 0;
        return 0;
    }
     
    if ((port_ptr->port_oper_info.media_type != MEDIA_TYPE_1000BASE_EMPTY))
    {
    	if (port_ptr->port_mtype == GIG_FIBER_PORT ||
        	port_ptr->port_mtype == X10GIG_FIBER_PORT ||
	    	(is_qsfp_optictype_on_x40g(DM_lport) && is_qsfp_optic_dom_supported(DM_lport))){
          		int index = get_sfp_monitor_from_port(DM_lport);
				sfp_monitor_set_interval(index, DM_lport);
				return 1;
		}
				//sfp_monitor_data_init(DM_lport);
    }

	return 0;

}

/*	Below API would be placed in the recieving thread of FI which will collect updated 
 *	sfp_monitor data and do the requisite operation.
 * */

void logging_dom_info(struct om_port_data om_data)
{
	PORT_ID port = om_data.port;
	int i;
	char msg[64];

	tanto_sfp_monitor_data_init(port, om_data.threshold_data);

	
	
	if(om_data.threshold_error == 1){
		//log error
    	if (debugGlobal.system.optics)
        	uprintf("OPTICAL MONITORING: THRESHOLDS READ FAILED, port %p\n", port);

    	if (IS_PORT_UP(port) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set
        	|| (SPTR_PORT_DB(port))->port_config.dom_down_port_set) {
        	
			ksprintf(cu_line_buf, "OPTICAL MONITORING: THRESHOLDS READ FAILED, port %p\n", port);
        	send_port_optical_msg_type(cu_line_buf, OPTICAL_MONITORING_MSG_TYPE);
		}
	}
	
	
	

	
	if(om_data.optic_error = 1){
		//log error

		if (debugGlobal.system.optics) {
			for(i = 0; i < QSFP_MON_INT_LEN; i++)
				uprintf("msg[%d]=%x\n", i, msg[i]);
		}
		

			ksprintf(cu_line_buf, "OPTICAL MONITORING: port %p, failed to read latched flags\n", port);
			send_port_optical_msg_type(cu_line_buf, OPTICAL_MONITORING_MSG_TYPE);

	}	
	char buff[64];
	if(SPTR_PORT_DB(port)->port_mtype == X40G_STACK_PORT)
	{
		buff[0] = (om_data.optic_data[3]&0xc0) | ((om_data.optic_data[4] & 0xc0)>>2); // temp, volt alarm
		buff[1] = (om_data.optic_data[8]&0xc0)& 0xff; // Bias alarm chan 1
		buff[2] = (om_data.optic_data[6]&0xc0) & 0xc0; // RX power alarm chan 1
		buff[3] = ((om_data.optic_data[3]& 0x30)<<2) | (om_data.optic_data[4] & 0x30); // temp, volt warning
		buff[4] = ((om_data.optic_data[8]&0x30)<<2)& 0xc0; // Bias warning chan 1
		buff[5] = ((om_data.optic_data[6]&0x30)<<2) & 0xc0; // RX power warning chan 1
		
		tanto_sfp_optical_monitoring(port, buff);
	
	}else{
		tanto_sfp_optical_monitoring(port, om_data.optic_data);
	}


}




void tanto_sfp_monitor_data_init(PORT_ID lport, char* buffer)
{
	UINT8 optic_data[6] = {0};
	enum SFP_MONITOR_DEVICE_TYPE sfp_type_flag = 0;
	int index=0;
	int found=0;
	int stackId = PORT_TO_STACK_ID(lport);

	if(SPTR_PORT_DB(lport)->port_mtype == GIG_FIBER_PORT) 
	{
		if(!dom_non_brocade_on_off_set && !(SPTR_PORT_DB(lport))->port_config.dom_non_brocade_set) // if non-brocade support, 
		sfp_type_flag = is_sfp_capable_of_optical_monitoring(lport);
		if(sfp_type_flag != SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE)
			goto sfp_not_monitorable;
	}


	SPTR_PORT_DB(lport)->port_oper_info.dom_capable = 1;

  index = get_sfp_monitor_from_port(lport);
  if(debugGlobal.system.optics || debugGlobal.system.mibdom)
    uprintf("SFP/SFP+/QSFP+ port %p assigned sfp_monitor index=%d media type=%xx\n", lport, 
    index, SPTR_PORT_DB(lport)->port_oper_info.media_type);

	if (sfp_monitor[stackId][index].Optical_monitor_use != OPTICS_IN_MONITOR)
	{
		sfp_monitor[stackId][index].Optical_monitor_use = OPTICS_IN_MONITOR;
		sfp_monitor[stackId][index].inited = SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE;
		sfp_monitor[stackId][index].sfp_port = lport;
		sfp_monitor[stackId][index].enable0 = 0xFFC00000;
		sfp_monitor[stackId][index].enable1 = 0xFFC00000;
		sfp_monitor[stackId][index].flag0 = 0;
		sfp_monitor[stackId][index].flag1 = 0;
		
		/*Copy the threshold buffer from DOM to FI sfp_monitor[stackId][index].thresholds
		 * TODO: INDRA*/
			
		memcpy((UINT8 *)&sfp_monitor[stackId][index].thresholds, buffer, SFP_MONITOR_THRESHOLD_LEN);

	//	sfp_monitor_set_interval(index, lport);
	//	sfp_monitor_threshold_init(index, lport);
		/* Only use the sfp monitor data which would be updated through DOM process::
		 * In the existing design there is a catch : If thresold init fails then FI prints a message <Have to take care that> */
		if (debugGlobal.system.optics)
			kprintf("%s: interval %d, port %p: done.\n", __FUNCTION__,
					sfp_monitor[stackId][index].interval, lport);
	}

sfp_not_monitorable:
	if(IS_PORT_UP(lport) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set ||
		(SPTR_PORT_DB(lport))->port_config.dom_down_port_set)
	{
		if (sfp_type_flag == SFP_MONITOR_DEVICE_TYPE_NOT_FOUNDRY_QUALIFIED)
			optical_monitior_log_msg(NOT_FOUNDRY_OPTICS_MSG_TYPE, lport);

		else if(sfp_type_flag == SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE)
			optical_monitior_log_msg(OPTICAL_MONITORING_FOUNDRY_OPTICS_NOT_CAPABLE_MSG_TYPE, lport);
	}

	return;
}

/*	This API would be called as part of reciving thread in FI to give the syslog
 * */

void tanto_sfp_optical_monitoring (PORT_ID lport,char* buf)
{
 // UINT8 buf[64];
  UINT32 curr_flag0, curr_flag1, temp_flag0, temp_flag1;
  int index;
  int found = 0;
  int stackId = PORT_TO_STACK_ID(lport);

  if (stackId != MY_BOOTUP_STACK_ID)
    return;     

	
  OPTMON_DTRACE (("\nsfp_optical_monitoring() - index=%d port=%p interval=%d sfp_monitor_interval=%d\n", 
                  index, lport,
                  SPTR_PORT_DB (lport)->port_config.optical_monitor_interval, 
                  sfp_monitor[stackId][index].interval));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nsfp_optical_monitoring() - index=%d port=%p interval=%d sfp_monitor_interval=%d\n", 
                  index, lport,
                  SPTR_PORT_DB (lport)->port_config.optical_monitor_interval, 
                  sfp_monitor[stackId][index].interval);

/*Need to call the send API with om_enabled_port_list[] and perform the operation over DOM process
 * The rest of this function execution will happen when we recieve the message from DOM with updated value and 
 * proceeds further*/


  /* We need to gather the calibration value first for SNMP */ 
      if(SPTR_PORT_DB(lport)->port_oper_info.snmp_dom_set_flag==0) {
         sw_cu_get_calibration_measure(lport);
           SPTR_PORT_DB(lport)->port_oper_info.snmp_dom_set_flag = 1;
      }

/*
  if (check_set_interval (SFP_OPTICS, index, lport) == 0)
  {
    return;
  }


  if(read_sfp_interrupt_flags(lport,buf) != LINK_OK)
    return;	
*/
/*Dont call these 2 APIs here but decide from the updated buf from DOM
 * SFP_MONITOR_FLAG_OFFSET would be read and sent back here TODO: Indra*/

  temp_flag0 = *(UINT32 *) buf;
  temp_flag1 = *(UINT32 *) (buf + 4);


/*Below code can be enabled : when we need a double check on the DOM processed data
 * */

/*Do the same repetitive iterative operation in DOM to take decision*/

/*
  if((temp_flag0) || (temp_flag1))
  {
*/	  /* The SFP tends to generate momentary latches during optic initialization.
	  Before sending syslog messages, read again to confirm if the latches generated are genuine */
/*	  if(read_sfp_interrupt_flags(lport, buf) != LINK_OK)
		  return;
  
	  curr_flag0 = *(UINT32 *)buf;
	  curr_flag1 = *(UINT32 *) (buf + 4);
 */
	  /* Allow only those latches which were set both the times */
/*	  
	  curr_flag0 &= temp_flag0;	
	  curr_flag1 &= temp_flag1;
  }
  else 
  {
    curr_flag0 = temp_flag0;
    curr_flag1 = temp_flag1;
  }
*/
	curr_flag0 = temp_flag0;
	curr_flag1 = temp_flag1;

  curr_flag0 &= sfp_monitor[stackId][index].enable0;
  curr_flag1 &= sfp_monitor[stackId][index].enable1;

  sfp_monitor_display_flag (index, lport, curr_flag0, curr_flag1);

  sfp_monitor[stackId][index].flag0 = curr_flag0;
  sfp_monitor[stackId][index].flag1 = curr_flag1;

  OPTMON_DTRACE (("\nsfp_optical_monitoring() - sfp_monitor[%d].flag0 =%d, flag1= %d\n", 
                     index, curr_flag0, curr_flag1));

   fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nsfp_optical_monitoring() - sfp_monitor[%d].flag0 =%d, flag1= %d\n", 
                     index, curr_flag0, curr_flag1);
   
}

static unsigned int icx7650_get_sfp_monitor_from_port(PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);
  int stack_module = PORT_TO_MODULE_ID_LOCAL(port);
  int module_port = PORT_TO_MODULE_PORT(port);

  return module_port;
}



int
check_set_interval (int media, int index, PORT_ID lport)
{
  int stackId = PORT_TO_STACK_ID(lport);
  extern UINT32 gi_board_type;

  if (SPTR_PORT_DB (lport)->port_config.optical_monitor_interval == 0)
  {
    return 0;
  }

  if (media == SFP_OPTICS)
  {
    if (sfp_monitor[stackId][index].interval > 
        SPTR_PORT_DB(lport)->port_config.optical_monitor_interval * 60)
    {
      sfp_monitor_set_interval (index, lport);
    }

  /* SideWinder runs 4 ports per interval. It takes 14 cycles to next run for 48F/48C, 7 cycles for 26Q */
  if(IS_SIDEWINDER()) {
    if(gi_board_type == SW_BOARD_TYPE_20QXG)
      sfp_monitor[stackId][index].interval -= OpticalMonitor_SFP_timer * 7;
    else
      sfp_monitor[stackId][index].interval -= OpticalMonitor_SFP_timer * 14;
	}
  else
    sfp_monitor[stackId][index].interval -= OpticalMonitor_SFP_timer * 4;
    if ((long)sfp_monitor[stackId][index].interval > 0)
    {
      OPTMON_DTRACE (("check_set_interval() SFP %p - sfp_monitor interval=%d\n", 
                lport, sfp_monitor[stackId][index].interval));

	  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: check_set_interval() SFP %p - sfp_monitor interval=%d\n", 
                lport, sfp_monitor[stackId][index].interval);
   
      return 0;
    }

  /* We only need to gather calibration data after interval expired */
    if (debugGlobal.system.mibdom)
       uprintf("%d seconds reset interval SFP %p - sfp_monitor interval=%d\n", g_time_counter/10, lport, sfp_monitor[stackId][index].interval);
    sw_cu_get_calibration_measure(lport);

    sfp_monitor_set_interval (index, lport);
    return 1;
  }

  return 0;
}

static void
sfp_get_one_port_optic_power (PORT_ID port, qsfpp_port_optic_t *port_optic)
{
  UINT8 media_type = 0;
  int stackId = PORT_TO_STACK_ID(port);
  PORT_ID stack_port = PORT_TO_STACK_PORT(port);

  if (!IS_PORT_DB_VALID (port))
    return;

  if (stackId != MY_BOOTUP_STACK_ID)
    return LINK_OK;

  media_type = SPTR_PORT_DB (port)->port_oper_info.media_type;

  if(SPTR_PORT_DB(port)->port_mtype == X10GIG_FIBER_PORT || SPTR_PORT_DB(port)->port_mtype == GIG_FIBER_PORT) 
  { 
    sfpp_sfp_get_measure(port, port_optic);
    return;
  }

  sfp_get_temperature (port, (signed short *) (&port_optic->temp_reg));
  sfp_get_voltage (port, (UINT16 *) (&port_optic->voltage));
  sfp_get_tx_bias_current (port, (UINT16 *) (&port_optic->tx_bias_current_reg));
  sfp_get_rx_power (port, (UINT16 *) (&port_optic->rx_power_reg));
  sfp_get_thresholds (port, (void *) (&port_optic->thresholds));

  if(!is_qsfp_optic_dom_supported(port))
  {
    sfp_get_tx_power(port, (unsigned short *)(&port_optic->tx_power_reg));
  }
  else
  {
    qsfp_get_volt(port, (UINT16 *)(&port_optic->voltage));
    qsfp_get_rx2to4_power(port, (unsigned short *)(&port_optic->rx2_power_reg));
    qsfp_get_tx2to4_bias(port, (unsigned short *)(&port_optic->tx2_bias_reg));
  }
}

int
mw2dbm (UINT16 mwreg, UINT32 * val)
{
  int neg = 0;
  double dBm = ((double) mwreg) / 10000;

  dBm = log10 (dBm);

  if (dBm < 0)
  {
    neg = 1;
    dBm = -1 * dBm;
  }
  *val = (UINT32) (dBm * 100000);
  return neg;
}


void 
show_stack_sfp_optic_temp_power(PORT_ID sfp_lport, UINT8 msg_type)
{
  int stackId =  PORT_TO_STACK_ID(sfp_lport);
  UINT32 record_size = sizeof(pp_stack_port_optic_ipc_header);
  pp_stack_port_optic_ipc_header *pp_port_optic_header = NULL;

  if(debugGlobal.system.mibdom)
    kprintf("\nstart show_stack_sfp_optic_temp_power() - Sent %s display to unit %u, size=%u, port=%p\n",
      (msg_type==PP_PORT_SHOW_SFP_OPTIC_ON_STACK_PORT)?"dom":"threshold",stackId, record_size, sfp_lport);

  if(STACK_AM_I_SLAVE)
    return;

  if(stackId == MY_BOOTUP_STACK_ID)
    return;

  set_parser_wait_until_callback();	/* don't process next command */

  pp_port_optic_header = (pp_stack_port_optic_ipc_header *)dy_malloc(record_size);

  pp_port_optic_header->type = msg_type;
  pp_port_optic_header->stackId = stackId;
  pp_port_optic_header->port_id = sfp_lport;

  rel_ipc_send_msg(stackId, REL_IPC_CHAN_BASE, IPC_MSGTYPE_CHASSIS_OPERATION, pp_port_optic_header, record_size);

  if(debugGlobal.system.mibdom)
    kprintf("end   show_stack_sfp_optic_temp_power() - Sent %s display to unit %u, size=%u, port=%p\n",
      (msg_type==PP_PORT_SHOW_SFP_OPTIC_ON_STACK_PORT)?"dom":"threshold",stackId, record_size, sfp_lport);

  dy_free(pp_port_optic_header);

  if (stackId != MY_BOOTUP_STACK_ID )
    g_uprintf_dest_save = g_uprintf_dest;

  return;
}

static qsfpp_port_optic_t sfp_port_optic_into;


//sfp_lport is global port
void
hal_show_sfp_optic_temp_power (PORT_ID sfp_lport /*, int head */)
{
  qsfpp_port_optic_t *port_optic;
  int i, index, temp, precision;
  UINT32 val;
  int found = 0;
  int valid_rx_pwr = 0,valid_bias = 0;
  int stackId = PORT_TO_STACK_ID(sfp_lport);
  int max_sfp = 0;

#if 0
	if(SPTR_PORT_DB(sfp_lport)->port_mtype == X10GIG_FIBER_PORT) { // It is 10G 
		if(!is_xfp_capable_of_optical_monitoring(sfp_lport)) { // non-Brocade XFP, SFPP
			if(!dom_non_brocade_on_off_set) // not set for non-Brocade optics
				return;
		}
	} else if(SPTR_PORT_DB(sfp_lport)->port_mtype == GIG_FIBER_PORT) { // it is 1G fiber
		if(is_sfp_capable_of_optical_monitoring(sfp_lport) != SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE) {
			if(!dom_non_brocade_on_off_set) // if 1G fiber other vendors, and not set it
				return;
		}
	}
#endif 
	if(!(dm_debug_mask&DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
		if(!IS_PORT_UP(sfp_lport) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(sfp_lport))->port_config.dom_down_port_set)  // If Brocade optics but no debugging 
			return;

  index = get_port_sfp_monitor_index(sfp_lport);
  if (index == -1) 
  {
    debug_uprintf ("%s:%d: can not find sfp_monitor for port %p\n", 
                     __FILE__, __LINE__, sfp_lport);
    return;
  }

  port_optic = (qsfpp_port_optic_t *)dy_malloc(sizeof(qsfpp_port_optic_t));

  if (port_optic == NULL)
    return;

  memset((void *)port_optic, 0x0, sizeof(qsfpp_port_optic_t));

  if (stackId == MY_BOOTUP_STACK_ID)
  {
    sfp_get_one_port_optic_power(sfp_lport, port_optic);
  }
  else 
  {
    port_optic->temp_reg = sfp_port_optic_into.temp_reg;
    port_optic->voltage = sfp_port_optic_into.voltage;
    port_optic->tx_bias_current_reg = sfp_port_optic_into.tx_bias_current_reg;
    port_optic->tx_power_reg = sfp_port_optic_into.tx_power_reg;
    port_optic->rx_power_reg = sfp_port_optic_into.rx_power_reg;
    port_optic->thresholds = sfp_port_optic_into.thresholds;
    port_optic->rx2_power_reg = sfp_port_optic_into.rx2_power_reg;
    port_optic->rx3_power_reg = sfp_port_optic_into.rx3_power_reg;
    port_optic->rx4_power_reg = sfp_port_optic_into.rx4_power_reg;
    port_optic->tx2_bias_reg = sfp_port_optic_into.tx2_bias_reg;
    port_optic->tx3_bias_reg = sfp_port_optic_into.tx3_bias_reg;
    port_optic->tx4_bias_reg = sfp_port_optic_into.tx4_bias_reg;
  }

  if (stackId != MY_BOOTUP_STACK_ID )
    g_uprintf_dest = g_uprintf_dest_save;

  if ((SHOW_BUF = sv_buf_alloc()) != NULL)
    redirect_output_to_display_buf();

  if (port_optic->temp_reg == 0) 
  {
    if ((dm_debug_mask & DM_OPTICAL_MONITOR_QA))
      uprintf("%s, lport=%p\n", __FUNCTION__, sfp_lport);

    uprintf("port %p is not capable of digital monitoring.\n", sfp_lport);
    goto noprint_not_capable;
  }

  if(stackId == MY_BOOTUP_STACK_ID) 
  if(!optical_monitor_data_ready(sfp_lport)) 
  {
    goto noprint_not_capable;
  }
  if(is_qsfp_optictype_on_x40g(sfp_lport) && is_qsfp_optic_dom_supported(sfp_lport))
		display_40G_optic_power_head((UINT8)SPTR_PORT_DB(sfp_lport)->port_oper_info.media_type);
  else {
	  uprintf (" Port   Temperature    Voltage       Tx Power      Rx Power    Tx Bias Current\n");
	  uprintf ("+-----+-------------+-------------+-------------+-------------+---------------+\n");
  	}

  uprintf ("%p", sfp_lport);

  show_sfp_optic_power_helper(sfp_lport, port_optic, index, &valid_rx_pwr, &valid_bias);

  if (is_qsfp_optictype_on_x40g(sfp_lport))
    show_40G_optic_power_helper(sfp_lport, port_optic, index, valid_rx_pwr, valid_bias);
  else
    uprintf("\n\r\n");

noprint_not_capable:
  dy_free (port_optic);

  if (SHOW_BUF != NULL)
    sv_buf_display();

  if (stackId != MY_BOOTUP_STACK_ID)
    print_prompt(&cdbs[g_uprintf_dest]);

  return;
}

static void
optical_monitior_log_msg (enum SYS_LOG_MSG_TYPE log_msg_type, int module_port_index)
{
  extern char cu_line_buf[];
  char *cu_line_ptr = cu_line_buf;
  int trap_count = 0;

  if(!IS_PORT_UP(module_port_index)) {
	// BUG: 98716 we trap messages only if the port is up
	return;
  }

  trap_count = (SPTR_PORT_DB(module_port_index)->port_config.optical_monitor_interval * 60) / OpticalMonitor_sche;

  OPTMON_DTRACE (("optical_monitior_log_msg()- port=%p, interval=%d port_trap_count=%d trap_count=%d\n",
                  module_port_index,
                  SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_interval,
                  SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_trap_count, trap_count));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: optical_monitior_log_msg()- port=%p, interval=%d port_trap_count=%d trap_count=%d\n",
                  module_port_index,
                  SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_interval,
                  SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_trap_count, trap_count);

  if (SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_trap_count <= 0)
  {
    SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_trap_count = trap_count;

    switch (log_msg_type)
    {
    case NOT_FOUNDRY_OPTICS_MSG_TYPE:
      ksprintf (cu_line_ptr, "SYSTEM: Optic is not Brocade qualified (port %p).\n", 
                              module_port_index);
      send_port_optical_msg_type (cu_line_ptr, NOT_FOUNDRY_OPTICS_MSG_TYPE);
      break;

    case OPTICAL_MONITORING_NOT_FOUNDRY_OPTICS_MSG_TYPE:
      ksprintf (cu_line_ptr,
                "OPTICAL MONITORING: Optic is not Brocade qualified, "
                "optical monitoring is not supported (port %p).\n",
                module_port_index);
      send_port_optical_msg_type (cu_line_ptr, OPTICAL_MONITORING_NOT_FOUNDRY_OPTICS_MSG_TYPE);
      break;

    case OPTICAL_MONITORING_FOUNDRY_OPTICS_NOT_CAPABLE_MSG_TYPE:
      ksprintf (cu_line_ptr, "OPTICAL MONITORING: port %p is not capable.\n", module_port_index);
      send_port_optical_msg_type (cu_line_ptr, OPTICAL_MONITORING_FOUNDRY_OPTICS_NOT_CAPABLE_MSG_TYPE);
      break;

    default:
      return;
    }
  }

  SPTR_PORT_DB (module_port_index)->port_config.optical_monitor_trap_count--;

}

static enum SFP_MONITOR_DEVICE_TYPE
is_sfp_capable_of_optical_monitoring (PORT_ID port)
{
  UINT8 vendor_data[SFP_MONITOR_VENDOR_DATA_LEN1];
  UINT8 vendor_name[SFP_VENDOR_NAME_LEN];
  UINT8 message1[64];
  UINT8 fdry[4];
  UINT8 type;
  UINT8 addr6, addr5, addr12, addr14;
  UINT16 wavelength;
  UINT8 vendor_oui[3];
  
  memset (&vendor_data[0], 0x0, SFP_MONITOR_VENDOR_DATA_LEN1);

  if (sfp_media_read (port, SFP_EEPROM_ADDR, 
                      0, SFP_MONITOR_VENDOR_DATA_LEN1, 
                      vendor_data) != SFP_MONITOR_VENDOR_DATA_LEN1)
  {
    OPTMON_DTRACE ((" VENDOR DATA read error - port = %p\n", port));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  VENDOR DATA read error - port = %p\n", port);
	
    return SFP_MONITOR_DEVICE_TYPE_UNKNOWN;
  }

  addr5 = vendor_data[5];
  addr6 = vendor_data[6];
  addr12 = vendor_data[12];
  addr14 = vendor_data[14];
  memcpy (&vendor_name[0], &vendor_data[20], SFP_VENDOR_NAME_LEN);
  memcpy (&wavelength, &vendor_data[60], 2);
  memcpy (&type, &vendor_data[92], 1);
  memcpy(vendor_oui, vendor_data+BROCADE_OUI_LOCATION, 3);

  OPTMON_DTRACE (("addr5=%x, addr6=%x, addr12=%x, addr14=%x, wavelen=%x vendor=%s DM type=%x\n",
                  addr5, addr6, addr12, addr14, wavelength, &vendor_name[0], vendor_data[92]));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  addr5=%x, addr6=%x, addr12=%x, addr14=%x, wavelen=%x vendor=%s DM type=%x\n",
                  addr5, addr6, addr12, addr14, wavelength, &vendor_name[0], vendor_data[92]);

  // Check if Foundry Networks names exist for new SX, LX and BXU, BXD, SX2 etc.
  if (((strncmp (vendor_name, "FOUNDRY NETWORKS", SFP_VENDOR_NAME_LEN)) &&
      (strncmp (vendor_name, "Foundry Networks", SFP_VENDOR_NAME_LEN))) &&
      ((vendor_oui[0] != BROCADE_OUT_BYTE0) && 
        (vendor_oui[1] != BROCADE_OUT_BYTE1) &&
        (vendor_oui[2] != BROCADE_OUT_BYTE2)))
  {
    	OPTMON_DTRACE (("SFP in port %d Not qualified\n", port));

		fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  SFP in port %d Not qualified\n", port);
		
    	return SFP_MONITOR_DEVICE_TYPE_NOT_FOUNDRY_QUALIFIED;
  }

  // SFP standard at byte 6 should be 0x40 for BXU, BXD, Foundry has 0x42, 07/30/07
  if (                          // legacy SX2
       !(((addr6 == 0x01) || (addr6 == 0x20) || (addr6 == 0x10)) && (wavelength == 0x051E))     /* SX2 release 2.1 */
       && !(((addr6 == 0x40) || (addr6 == 0x42)) && (addr12 == 0x0D) && (addr14 == 0x0A) && 
            ((wavelength == 0x051E) || (wavelength == 0x05D2))) /* 1GE-BXD/BXU */
       && !((addr6 == 0x20) && (wavelength == 0x051E) && 
            (((addr5 == 2) && (addr14 == 0xF)) || ((addr5 == 4) && (addr14 == 0x28))))     /*100-FX-IR/LR */
       && !(addr6 == 0)         /* OC */
    )
  {
    // content "DD" or "dd"
    memset (&fdry[0], 0x0, 4);
    if (sxs_sfp_read (port, GBIC_DIAG_MONITOR_FDRY, (UINT8 *) & fdry[0], GBIC_DIAG_MONITOR_FDRY_LEN) !=
        GBIC_DIAG_MONITOR_FDRY_LEN)
    {
      OPTMON_DTRACE (("OPTICAL MONITORING: Digital Diag Read Failed, port %p\n", port));

	  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  OPTICAL MONITORING: Digital Diag Read Failed, port %p\n", port);
	  
      return SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE;
    }

    OPTMON_DTRACE (("Optical Signature is =%s port=%p\n", &fdry[0], port));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  Optical Signature is =%s port=%p\n", &fdry[0], port);

    // New types of SX, LX, etc. check the "DD" or "dd"
    if (strncmp (fdry, "DD", GBIC_DIAG_MONITOR_FDRY_LEN) && 
        strncmp (fdry, "dd", GBIC_DIAG_MONITOR_FDRY_LEN))
    {
      return SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE;
    }
  }

  if (!(type & 0x40))
  {
    OPTMON_DTRACE (("OPTICAL MONITORING: DD not implemented - type %x failure, port %p\n", 
                    vendor_data[92], port));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  OPTICAL MONITORING: DD not implemented - type %x failure, port %p\n", 
                    vendor_data[92], port);
	
    return SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE;
  }

  if (!(type & 0x20))
  {
    OPTMON_DTRACE (("OPTICAL MONITORING: internally calibrated not implemented - "
                    "type %x failure, port %p\n",
                    vendor_data[92], port));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  OPTICAL MONITORING: internally calibrated not implemented - "
                    "type %x failure, port %p\n",
                    vendor_data[92], port);
	
    return SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE;
  }

  OPTMON_DTRACE ((" Brocade Optical Monitor OK port = %p\n", port));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO:  Brocade Optical Monitor OK port = %p\n", port);
  
  return SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE;
}

void
hal_sfp_monitor_zap_data (PORT_ID sfp_lport)
{
  int index = 0;
  int stackId = PORT_TO_STACK_ID(sfp_lport);

  index = get_port_sfp_monitor_index(sfp_lport);
  if (index != -1)
    memset (&sfp_monitor[stackId][index], 0x0, sizeof (SFP_MONITOR));

}

static void
op_zap_per_port_data (PORT_ID lport)
{
  hal_sfp_monitor_zap_data (lport);
}

void
hal_cu_optic_mon_all_ports_zap_data (void)
{
  int index;
  int stackId =1;

  for(stackId = 1; stackId < MAX_SYS_UNIT_NUM +1; stackId ++) {
	if(IS_STACK_LOGICAL(stackId))
		continue;
    int max_num_sfp_monitor = get_num_sfp_monitors(stackId);

    for(index = 0; index < max_num_sfp_monitor; index++)
      memset(&sfp_monitor[stackId][index], 0x0, sizeof(SFP_MONITOR));
  }
}

void
hal_sw_op_mon_module_remove (int OMSlotNum)
{
  int iom;
  PORT_ID DM_lport = 0;

  DM_lport = MAKE_PORTID (OMSlotNum, 0);
  for (iom = 0; iom < g_module[OMSlotNum].number_of_ports; iom++, DM_lport++)
  {
    op_zap_per_port_data (DM_lport);
  }
}

void
hal_optical_monitor (void)
{
  int iom, OMSlotNum;
  PORT_ID lport = 0, DM_lport = 0;
  PORT_DB *port_ptr;
  int stackId = 1, stack_slot =0;	
  int p, max_port_serv, monitor_slot;

  if (IS_SIDEWINDER()) {
     icx7750_optical_monitor();
     return;
  }

  for (OMSlotNum = 0; OMSlotNum < g_hw_info.max_mod; OMSlotNum++)
  {
    stackId = MODULE_TO_STACK_ID(OMSlotNum);
    stack_slot = MODULE_TO_STACK_MODULE(OMSlotNum);

    if (stackId != MY_BOOTUP_STACK_ID) continue;

    if (!(IS_CHASSIS_STACK_EXIST(stackId) && is_module_exist(OMSlotNum))) continue;

    monitor_slot = om_scheduler_get_serv_slot(&monitor);
    if (monitor_slot != stack_slot) continue;

    if (debugGlobal.system.optics)
    {
      om_scheduler_show(&monitor);
      uprintf("%s: serve slot %d\n", __FUNCTION__, monitor_slot);
    }

    max_port_serv = om_scheduler_get_slot_ports_per_cycle(&monitor, stack_slot);

    for (p = 0; p < max_port_serv; p++)
    {
      int module_port = om_scheduler_get_serv_port(&monitor);
      PORT_ID DM_lport = MAKE_PORTID(OMSlotNum, module_port);

      om_scheduler_update(&monitor);

      if (!IS_PORT_DB_VALID (DM_lport))
        continue;

      port_ptr = SPTR_PORT_DB (DM_lport);
      if (port_ptr->port_config.optical_monitor_interval == 0)
        continue;

      if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
        if (!IS_PORT_UP(DM_lport) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(DM_lport))->port_config.dom_down_port_set)
          continue;

      if (!hal_is_xfp_sfp_present(DM_lport))
      {
        op_zap_per_port_data(DM_lport);
        continue;
      }

      if (port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CX4)
        continue;

      if ((port_ptr->port_mtype == GIG_FIBER_PORT && 
           port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_UNKNOWN) ||
          (port_ptr->port_mtype == X10GIG_FIBER_PORT && 
           port_ptr->port_oper_info.media_type == LINK_MEDIA_INVALID))
        continue;

#if 0
      if(port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHA)
        if(port_ptr->port_oper_info.dom_capable != TRUE)
          continue;
#endif

      if (port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_TX || 
          port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_CX ||
          port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHB || 
          port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_COPPER || 
          port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_ACTIVE_COPPER || 
          port_ptr->port_oper_info.media_type == LINK_MEDIA_1G_TWINAX ||
          port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CABLE ||
          port_ptr->port_oper_info.media_type == LINK_MEDIA_1GBASE_ACTIVE_CABLE)
      {
        op_zap_per_port_data(DM_lport);
        port_ptr->port_config.optical_monitor_interval = 0;
        continue;
      }
     
      if ((port_ptr->port_oper_info.media_type != MEDIA_TYPE_1000BASE_EMPTY))
      {
        if (port_ptr->port_mtype == GIG_FIBER_PORT ||
            port_ptr->port_mtype == X10GIG_FIBER_PORT ||
	    (is_qsfp_optictype_on_x40g(DM_lport) && is_qsfp_optic_dom_supported(DM_lport)))
          sfp_monitor_data_init(DM_lport);
      }
    }

  } //for all modules
}


static void  
cu_set_stack_dom (int stackId)
{	
	UINT32 record_size = sizeof(pp_stack_dom_ipc_header);
	pp_stack_dom_ipc_header *pp_stack_dom_header = NULL;
	int i = 0;
	int max_num_sfp_monitor = get_num_sfp_monitors(stackId);

  if(debugGlobal.system.mibdom)
    kprintf("gtime %d start cu_set_stack_dom() - Sent PP_IPC_TYPE_SET_STACK_DOM to unit %u, size=%u\n",
      g_time_counter, stackId, record_size);

	if( stackId == MY_BOOTUP_STACK_ID)  // Slave stact only
		return;

	if(!IS_CHASSIS_STACK_EXIST(stackId))
		return;

	if(!STACK_AM_I_MASTER)
	{
#ifdef __PORT_EXTENSION__
		if (STACK_AM_I_STANDBY) return;
		if(!(STACK_AM_I_CB_AND_PE_NUM > 0)) return;
#else
		return;
#endif __PORT_EXTENSION__
	}
	
	pp_stack_dom_header = (pp_stack_dom_ipc_header *)dy_malloc(record_size);

	if(pp_stack_dom_header == NULL)
		return;
	
	pp_stack_dom_header->type = PP_IPC_TYPE_SET_STACK_DOM;
	pp_stack_dom_header->stackId = stackId;

	for(i = 0; i < max_num_sfp_monitor; i ++)
		pp_stack_dom_header->dom_config[i] = dom_config[stackId][i];

	rel_ipc_send_msg(stackId, REL_IPC_CHAN_BASE, IPC_MSGTYPE_CHASSIS_OPERATION, 
                        pp_stack_dom_header, record_size);
	
	rel_ipc_flush(stackId, REL_IPC_CHAN_BASE); // Must flush it right now.

  if(debugGlobal.system.mibdom)
    kprintf("gtime %d end   cu_set_stack_dom() - Sent PP_IPC_TYPE_SET_STACK_DOM to unit %u, size=%u\n",
      g_time_counter, stackId, record_size);

	dy_free(pp_stack_dom_header);
}

/*Look here*/

void 
stacking_hotswap_dom(int stackId)
{
  PORT_ID port, i, index;
  MODULE_ID base_module = STACK_TO_BASE_MODULE(stackId);
  int max_num_sfp_monitor = get_num_sfp_monitors(stackId);

  if(stackId > MAX_SYS_UNIT_NUM)
  {
    return;
  }

  if (stackId == MY_BOOTUP_STACK_ID)  // Slave stack only
    return;

  if (!IS_CHASSIS_STACK_EXIST(stackId))
    return;

  if (STACK_AM_I_MEMBER)
    return;

  // SFP dom_config init
  for (i = 0; i < max_num_sfp_monitor; i++) 
  {
    int module;

    port = get_port_from_sfp_monitor(stackId, i);	
    index = get_dom_config_from_sfp_monitor(i);
    module = PORT_TO_MODULE_ID(port);

    /* for some platforms we may have more sfp_monitors than needed, 
     * flag them with BAD_PORT_ID, so when this config is received by member/pe,
     * they know how to process it */
    if (!MODULE_EXIST(module) || !IS_PORT_DB_VALID(port)) 
        port = BAD_PORT_ID;

    // SW port Map
    dom_config[stackId][index].port_id = port;

    if (port == BAD_PORT_ID) continue;

    dom_config[stackId][index].interval = (SPTR_PORT_DB(port))->port_config.optical_monitor_interval;
    dom_config[stackId][index].dom_down_port_set = dom_down_port_on_off_set;
    dom_config[stackId][index].dom_non_brocade_set = dom_non_brocade_on_off_set;

//	if((port == 26) || (port == 28) || (port == 30) ||(port == 266) || (port == 268) || (port == 270))
//		kprintf("(C)stacking_hotswap_dom stackId %d port %p dom_config interval = %d SPTR_PORT_DB interval = %d\n",
//		  stackId, port, dom_config[stackId][index].interval,(SPTR_PORT_DB(port))->port_config.optical_monitor_interval);

    if (dom_config[stackId][index].interval != 0)
    {
      sfp_monitor[stackId][i].Optical_monitor_use = OPTICS_IN_MONITOR;
      sfp_monitor[stackId][i].inited = SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE;
      sfp_monitor[stackId][i].sfp_port = port;
      sfp_monitor[stackId][i].enable0 = 0xFFC00000;
      sfp_monitor[stackId][i].enable1 = 0xFFC00000;
      sfp_monitor[stackId][i].flag0 = 0;
      sfp_monitor[stackId][i].flag1 = 0;
      sfp_monitor_set_interval(i, port);
    }
  }

  cu_set_stack_dom (stackId);
  return;
}

/* called from master */
void 
set_port_stack_optical_monitor_interval(UINT32 optical_monitor_intervalm, PORT_ID port)
{
  int index = 0;
  int stackId = PORT_TO_STACK_ID(port);
  PORT_ID stack_port = PORT_TO_STACK_PORT(port) ;
  int max_num_sfp_monitor = get_num_sfp_monitors(stackId);

  if ((dm_debug_mask&DM_OPTICAL_MONITOR_QA) || debugGlobal.system.mibdom)
    kprintf("set_port_stack_optical_monitor_interval(): stackId %d port %p, Interval %d \n", 
             stackId, port, optical_monitor_intervalm); 

  if (!IS_CHASSIS_STACK_EXIST(stackId))
    return;

  if (stackId == MY_BOOTUP_STACK_ID)
    return;

  if (!STACK_AM_I_MASTER)
  {
#ifdef __PORT_EXTENSION__
    if(!(STACK_AM_I_CB_AND_PE_NUM > 0)) return;
#else
    return;
#endif __PORT_EXTENSION__
  }

  index = get_dom_config_from_port(port);
  if (index < 0)//coverity 43018
	return;

  dom_config[stackId][index].interval = optical_monitor_intervalm;

  if(optical_monitor_intervalm)
  {
      int mon_index = get_sfp_monitor_from_port(port);
      if (index < max_num_sfp_monitor) 
      {
        sfp_monitor[stackId][mon_index].Optical_monitor_use = OPTICS_IN_MONITOR;
        sfp_monitor[stackId][mon_index].inited = SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE;
        sfp_monitor[stackId][mon_index].sfp_port = port;
        sfp_monitor[stackId][mon_index].enable0 = 0xFFC00000;
        sfp_monitor[stackId][mon_index].enable1 = 0xFFC00000;
        sfp_monitor[stackId][mon_index].flag0 = 0;
        sfp_monitor[stackId][mon_index].flag1 = 0;

        sfp_monitor_set_interval(mon_index, port);		
      } 
  }

//	if((port == 26) || (port == 28) || (port == 30) ||(port == 266) || (port == 268) || (port == 270))
//		kprintf("(B)stacking_hotswap_dom stackId %d port %p interval = %d\n",
//		  stackId, port, (SPTR_PORT_DB(port))->port_config.optical_monitor_interval);

  cu_set_stack_dom (stackId);
}

int
optmon_sfp_dump (void)
{
  int index;
  int stackId = MY_BOOTUP_STACK_ID;
  int max_num_sfp_monitor = get_num_sfp_monitors(stackId);

  for (index = 0; index < max_num_sfp_monitor; index++)
  {
    kprintf ("(%02d) sfp_port = %d\n", index, sfp_monitor[stackId][index].sfp_port);
    kprintf ("(%02d) inited = %d\n", index, sfp_monitor[stackId][index].inited);
    kprintf ("(%02d) enabled0 = %d\n", index, sfp_monitor[stackId][index].enable0);
    kprintf ("(%02d) enabled1 = %d\n", index, sfp_monitor[stackId][index].enable1);
    kprintf ("(%02d) interval = %d\n", index, sfp_monitor[stackId][index].interval);
    kprintf ("(%02d) Optical_monitor_use = %d\n", 
                 index, sfp_monitor[stackId][index].Optical_monitor_use);
  }

  return 0;
}


//this function is used for showing temp and power
int 
hal_cu_show_port_optic_temp_power(PORT_ID optic_port_id, UINT32 is_slot, UINT32 head)
{
  PORT_DB *port_ptr;
  int stackId = PORT_TO_STACK_ID(optic_port_id);
  int optic_slot = PORT_TO_MODULE_ID (optic_port_id);

  if (!MODULE_EXIST(optic_slot))
    return 0;

  if (is_slot)
  {
    int i=0, optic_slot;

    optic_slot = optic_port_id;
    optic_port_id = MAKE_PORTID(optic_port_id, 0);

    for (i=0; i<g_module[optic_slot].number_of_ports; i++, optic_port_id++)
    {
      if (!IS_PORT_DB_VALID(optic_port_id))
        continue;
		
	
      if (!is_xfp_sfp_present(optic_port_id))
        continue;

      if (SPTR_PORT_DB(optic_port_id)->port_oper_info.media_type == LINK_MEDIA_10GBASE_CX4) {
        uprintf("\nPort %p is 10G Copper and not capable of optical monitoring.\n", optic_port_id);
        continue;
      }

      if ((SPTR_PORT_DB(optic_port_id)->port_oper_info.media_type == MEDIA_TYPE_1000BASE_GBXU) ||
          (SPTR_PORT_DB(optic_port_id)->port_oper_info.media_type == MEDIA_TYPE_1000BASE_GBXD))
        continue;

      if (SPTR_PORT_DB(optic_port_id)->port_oper_info.media_type == MEDIA_TYPE_100BASE_FX_BIDI)
        continue;

      if (SPTR_PORT_DB(optic_port_id)->port_mtype == GIG_FIBER_PORT &&
          (SPTR_PORT_DB(optic_port_id)->port_oper_info.media_type != MEDIA_TYPE_1000BASE_EMPTY))
        show_sfp_optic_temp_power(optic_port_id);
      else
        uprintf("port %d is not a fiber port\n", optic_port_id);
    }
    return head;
  }

  if (is_port_stack_enabled(optic_port_id))
  {
    uprintf("port %p is a stacking port and not capable of digital optical monitoring.\n",
              optic_port_id);
    return 0;
  }

  if (!IS_PORT_DB_VALID (optic_port_id))
  {
    return head;
  }

	if(SPTR_PORT_DB(optic_port_id)->port_config.optical_monitor_interval == 0)
	{
		uprintf("Optical Monitoring disabled on port:%p\n",optic_port_id);
		return head;
	}

  if (!(dm_debug_mask & DM_OPTICAL_MONITOR_QA) && (!debugGlobal.system.optics) && (!debugGlobal.system.mibdom))
    if (!IS_PORT_UP(optic_port_id) && !dom_down_port_on_off_set && !(SPTR_PORT_DB(optic_port_id))->port_config.dom_down_port_set)
      return head;

  pp_link_media_assign(optic_port_id);

  port_ptr = SPTR_PORT_DB (optic_port_id);

	if (port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_GBXU ||
		port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_GBXD)
	{
		uprintf ("\nPort %p is 1G BXU or BXD and not capable of optical monitoring.\n", optic_port_id);
		return head;
	}

  if((port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_CX) ||
     (port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_TX) ||
     (port_ptr->port_oper_info.media_type == LINK_MEDIA_1G_TWINAX) ||
	 (port_ptr->port_oper_info.media_type == LINK_MEDIA_1GBASE_ACTIVE_CABLE))
  {
    uprintf ("\nPort %p is 1G Copper and not capable of optical monitoring.\n", 
                optic_port_id);
    return head;
  }    	
   
  if ((port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CX4) ||
      (port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_COPPER) ||
      (port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_ACTIVE_COPPER) ||
      (port_ptr->port_oper_info.media_type == LINK_MEDIA_10GBASE_CABLE) )
  {
    uprintf ("\nPort %p is 10G Copper and not capable of optical monitoring.\n", 
                optic_port_id);
    return head;
  }

	if(port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHB) {
		uprintf ("\nPort %p is LHB and not capable of optical monitoring.\n", 
				optic_port_id);
		return head;
	}

#if 0
  if(port_ptr->port_oper_info.media_type == MEDIA_TYPE_1000BASE_LHA) {
    if(port_ptr->port_oper_info.dom_capable != TRUE) {
      uprintf ("\nPort %p is non-OM LHA and not capable of optical monitoring.\n", optic_port_id);
      return head;
    }
  }
#endif

  if (IS_PP_40G_PORT(optic_port_id))
  {
    if (port_ptr->port_oper_info.media_type == LINK_MEDIA_40GBASE_BiDi) {
        uprintf("Port %p is 40G Bi-Directional XLPPI and not capable of optical monitoring.\n",
                 optic_port_id);
        return head;
    }
    else if (!is_sfp_sfpp_optictype_on_x40g(optic_port_id) &&
    		!is_qsfp_optic_dom_supported(optic_port_id))
    {
       uprintf ("\nPort %p is 40G Copper and not capable of optical monitoring.\n", 
                 optic_port_id);
       return head;
    }
  }

  if (port_ptr->port_oper_info.optic_type == PORT_OPER_INFO_OPTIC_TYPE_SFP ||
      port_ptr->port_oper_info.optic_type == PORT_OPER_INFO_OPTIC_TYPE_SFPP ||
      port_ptr->port_oper_info.optic_type == PORT_OPER_INFO_OPTIC_TYPE_QSFP)
  {
    if (stackId == MY_BOOTUP_STACK_ID)
      show_sfp_optic_temp_power(optic_port_id);
    else
      show_stack_sfp_optic_temp_power(optic_port_id, PP_PORT_SHOW_SFP_OPTIC_ON_STACK_PORT);	
  }

  return head;
}

/* to disable om for stacking port */
int 
stacking_port_om_disable()
{
	int i;
	PORT_ID lport = 0;
	int is_msg = 0;

	for (i = 0; i < 2; i++) {
		lport = STACKING_UNIT_STK_PORT(MY_BOOTUP_STACK_ID, i);
		if (IS_PORT_DB_VALID(lport)) {
			if ((SPTR_PORT_DB(lport))->port_config.
						optical_monitor_interval) {
				if (is_msg == 0) {
					uprintf ("Stacking is enabled. Optical Monitoring is not available for the port(s) %p", lport);
					is_msg = 1;
				} else {
					uprintf (", %p", lport);
				}
			}
			(SPTR_PORT_DB(lport))->port_config.optical_monitor_interval = 0;
			hal_sfp_monitor_zap_data(lport);
		}
	}

	if (is_msg == 1)
		uprintf ("\n");

	return CU_OK;
}

/* to enable om for stacking port */
int 
stacking_port_om_enable()
{
  int i;
  PORT_ID lport = 0;

  for (i = 0; i < 2; i++) 
  {
    lport = STACKING_UNIT_PRIMARY_PORT(MY_BOOTUP_STACK_ID, i);

    if (lport != PORT_INDEX_INVALID)
    {
      if (optical_monitor_interval != 0) // to global assigment
      {
      	if (IS_PORT_DB_VALID(lport))
		{
	        if ((SPTR_PORT_DB(lport))->port_config.optical_monitor_interval == 0)
	          (SPTR_PORT_DB(lport))->port_config.optical_monitor_interval = optical_monitor_interval;

	        (SPTR_PORT_DB(lport))->port_config.optical_monitor_trap_count = 0;
      	}
      }
    }
  }
  return CU_OK;
}

static int 
qsfp_read_monitoring_flags(PORT_ID port,unsigned char * buf)
{
	UINT8 msg[10];
	int i=0, rc=0;
	int stackId = PORT_TO_STACK_ID(port);
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);

	rc = sfp_media_read(port, SFP_EEPROM_ADDR, QSFP_MON_INT_FLAGS, QSFP_MON_INT_LEN, (UINT8 *)&msg[0]);
	if (rc != QSFP_MON_INT_LEN)
	{
		if (IS_PORT_UP(port) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set
			|| (SPTR_PORT_DB(port))->port_config.dom_down_port_set) 
		{
			ksprintf(cu_line_buf, "OPTICAL MONITORING: port %p, failed to read latched flags\n", port);
			send_port_optical_msg_type(cu_line_buf, OPTICAL_MONITORING_MSG_TYPE);
			return LINK_ERROR;
		}
	}
	if (debugGlobal.system.optics) {
		for(i = 0; i < QSFP_MON_INT_LEN; i++)
			uprintf("msg[%d]=%x\n", i, msg[i]);
	}

	if(SPTR_PORT_DB(port)->port_mtype == X40G_STACK_PORT)
	{
		buf[0] = (msg[3]&0xc0) | ((msg[4] & 0xc0)>>2); // temp, volt alarm
		buf[1] = (msg[8]&0xc0)& 0xff; // Bias alarm chan 1
		buf[2] = (msg[6]&0xc0) & 0xc0; // RX power alarm chan 1
		buf[4] = ((msg[3]& 0x30)<<2) | (msg[4] & 0x30); // temp, volt warning
		buf[5] = ((msg[8]&0x30)<<2)& 0xc0; // Bias warning chan 1
		buf[6] = ((msg[6]&0x30)<<2) & 0xc0; // RX power warning chan 1
	}
	return LINK_OK;

}

static int 
sfp_read_monitoring_flags(PORT_ID lport,unsigned char * buf)
{
	int i, status = LINK_OK;

	if(SPTR_PORT_DB(lport)->platform_port.is_fluffy_present == 1 && SPTR_PORT_DB(lport)->platform_port.is_optic_present == 1) 
		if(hal_fluffy_select_ext_lrm_optic(lport, FLUFFY_EEPROM_LRM_EXTERNAL) != LINK_OK)
			return LINK_ERROR;

	if(sfp_media_read(lport, SFP_EEPROM_ADDR_1, SFP_MONITOR_FLAG_OFFSET, 
								SFP_MONITOR_FLAG_LEN, buf) != SFP_MONITOR_FLAG_LEN)
		status = LINK_ERROR;
	else 
	{
		for(i = 0;;)
		{
			if(sfp_media_read(lport, SFP_EEPROM_ADDR_1, SFP_MONITOR_FLAG_OFFSET, 
								SFP_MONITOR_FLAG_LEN, buf) == SFP_MONITOR_FLAG_LEN)
				break;
			else if(i==5)
			{
				ksprintf(cu_line_buf, "OPTICAL MONITORING: port %p, failed to read latched flags\n", lport);
				send_port_optical_msg_type(cu_line_buf, OPTICAL_MONITORING_MSG_TYPE);
				status = LINK_ERROR;
			}
			i++;
		}
	}

	if(SPTR_PORT_DB(lport)->platform_port.is_fluffy_present == 1 && SPTR_PORT_DB(lport)->platform_port.is_optic_present == 1) 
		if(hal_fluffy_select_ext_lrm_optic(lport, FLUFFY_EEPROM_LRM_INTERNAL) != LINK_OK)
			return LINK_ERROR;

	return status;
}

static void 
show_sfp_optic_power_helper(PORT_ID port, qsfpp_port_optic_t *port_optic, 
                            int index, int *valid_rx1, int *valid_bias1)
{
	UINT32 val=0;
	int temp,precision;
	int invalid_temp=0,invalid_volt=0,invalid_tx_pwr=0,invalid_rx_pwr=0, invalid_bias=0;
	int stackId = PORT_TO_STACK_ID(port);

	// if no transceiver inserted, the temp reading count is 0
	if ((port_optic->temp_reg == -1) || (port_optic->temp_reg == 0))
	{
		uprintf("      N/A    ");
		invalid_temp=1;
	}
	else
	{
		temp = (int)port_optic->temp_reg>>8;
		if(port_optic->temp_reg & 0x8000)
			temp |= 0xffffff00;

		precision = (int)(((port_optic->temp_reg & 0xff) * 10000)/256);
		uprintf("  %3d.%04d C ",  temp, precision);
	}

	if ((port_optic->voltage == 0xffff) || (port_optic->voltage == 0))
	{
		uprintf("      N/A    ");
		invalid_volt=1;
	}
	else
	{
		uprintf("   %01d.%04d volts",  ((UINT16)(port_optic->voltage))/10000, ((UINT16)(port_optic->voltage))%10000);
	}
	
	if (!is_qsfp_optictype_on_x40g(port)) {
	if (port_optic->tx_power_reg == 0xffff || port_optic->tx_power_reg == 0x0)
	{
		uprintf("       N/A    ");
		invalid_tx_pwr=1;
	}
	else if (mw2dbm(port_optic->tx_power_reg, &val))
		uprintf("  -%01d.%04d dBm",  val/10000, val%10000);
	else
		uprintf("   %01d.%04d dBm",  val/10000, val%10000);
	}
	
	if (port_optic->rx_power_reg == 0xffff || port_optic->rx_power_reg == 0x0)
	{
		uprintf("      N/A    ");
		invalid_rx_pwr=1;
	}
	else if (mw2dbm(port_optic->rx_power_reg, &val))
		uprintf("   -%01d.%04d dBm",  val/10000, val%10000);
	else
		uprintf("    %01d.%04d dBm",  val/10000, val%10000);

	if (port_optic->tx_bias_current_reg == 0xffff || port_optic->tx_bias_current_reg == 0x0)
	{
		uprintf("     N/A  ");
		invalid_bias=1;
	}
	else
		uprintf("    %2d.%03d mA",  port_optic->tx_bias_current_reg/500, 
						(port_optic->tx_bias_current_reg*2)%1000);

	uprintf("\n");

	if (port_optic->temp_reg != -1 && port_optic->voltage != 0xffff &&
		port_optic->rx_power_reg  != 0xffff && port_optic->tx_bias_current_reg != 0xffff)
	{
		if (invalid_temp)
			uprintf("                ");
		else
		if (port_optic->temp_reg > sfp_monitor[stackId][index].thresholds.temp_high_alarm)
			uprintf("        High-Alarm");
		else
		if (port_optic->temp_reg >sfp_monitor[stackId][index].thresholds.temp_high_warn)
			uprintf("        High-Warn");
		else
		if (port_optic->temp_reg < sfp_monitor[stackId][index].thresholds.temp_low_alarm)
			uprintf("        Low-Alarm");
		else
		if (port_optic->temp_reg < sfp_monitor[stackId][index].thresholds.temp_low_warn)
			uprintf("        Low-Warn");
		else
			uprintf("        Normal  ");

		if (invalid_volt)
			uprintf("                ");
		else
		if (port_optic->voltage > sfp_monitor[stackId][index].thresholds.voltage_high_alarm)
			uprintf("        High-Alarm");
		else
		if (port_optic->voltage > sfp_monitor[stackId][index].thresholds.voltage_high_warn)
			uprintf("        High-Warn");
		else
		if (port_optic->voltage < sfp_monitor[stackId][index].thresholds.voltage_low_alarm)
			uprintf("        Low-Alarm");
		else
		if (port_optic->voltage < sfp_monitor[stackId][index].thresholds.voltage_low_warn)
			uprintf("        Low-Warn");
		else
			uprintf("        Normal  ");

		if (!is_qsfp_optictype_on_x40g(port)) {
		if (invalid_tx_pwr)
			uprintf("               ");
		else
		if (port_optic->tx_power_reg > sfp_monitor[stackId][index].thresholds.tx_power_high_alarm)
			uprintf("     High-Alarm");
		else
		if (port_optic->tx_power_reg > sfp_monitor[stackId][index].thresholds.tx_power_high_warn)
			uprintf("     High-Warn ");
		else
		if (port_optic->tx_power_reg < sfp_monitor[stackId][index].thresholds.tx_power_low_alarm)
			uprintf("     Low-Alarm ");	
		else
		if (port_optic->tx_power_reg < sfp_monitor[stackId][index].thresholds.tx_power_low_warn)
			uprintf("     Low-Warn  ");
		else
			uprintf("     Normal    ");
		}

		if (invalid_rx_pwr)
			uprintf("                ");
		else
		if (port_optic->rx_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_alarm)
			uprintf("     High-Alarm ");
		else
		if (port_optic->rx_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_warn)
			uprintf("     High-Warn  ");
		else
		if (port_optic->rx_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_alarm)
			uprintf("     Low-Alarm  ");
		else
		if (port_optic->rx_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_warn)
			uprintf("     Low-Warn   ");
		else
			uprintf("     Normal     ");

		if (invalid_bias)
			uprintf("               ");
		else
		if (port_optic->tx_bias_current_reg > sfp_monitor[stackId][index].thresholds.bias_high_alarm)
			uprintf("    High-Alarm");
		else
		if (port_optic->tx_bias_current_reg > sfp_monitor[stackId][index].thresholds.bias_high_warn)
			uprintf("    High-Warn ");
		else
		if (port_optic->tx_bias_current_reg < sfp_monitor[stackId][index].thresholds.bias_low_alarm)
			uprintf("    Low-Alarm ");
		else
		if (port_optic->tx_bias_current_reg < sfp_monitor[stackId][index].thresholds.bias_low_warn)
			uprintf("    Low-Warn  ");
		else
			uprintf("    Normal     ");
		*valid_bias1 = invalid_bias;
		*valid_rx1 = invalid_rx_pwr;
	}
}


static void 
display_40G_optic_power_head(UINT8 media_type)
{
	if(media_type == LINK_MEDIA_40GBASE_SR4)
		uprintf("\n                         40GBASE_SR4                            \n");
	else if(media_type == LINK_MEDIA_4X10G_SR4)
		uprintf("\n                          4X10G_SR4                            \n");
	else if(media_type == LINK_MEDIA_40GBASE_LM4)
		uprintf("\n                         40GBASE_LM4                            \n");
	else if(media_type == LINK_MEDIA_40GBASE_ESR4)
		uprintf("\n                         40GBASE_ESR4                            \n");
	else if(media_type == LINK_MEDIA_40GBASE_ER4)
		uprintf("\n                         40GBASE_ER4                            \n");
	else
		uprintf("\n                         40GBASE_LR4                            \n");
	uprintf("                       ===============                          ");
//	uprintf("\n Port  Temperature   Tx Power     Rx Power       Tx Bias Current\n");
	uprintf ("\n Port   Temperature    Voltage      Rx Power    Tx Bias Current\n");
	uprintf ("+-----+-------------+-------------+------------+---------------+\n");
}

static void 
show_40G_optic_power_helper(PORT_ID port, qsfpp_port_optic_t *port_optic, 
                            int index, int invalid_rx1, int invalid_bias1)
{
	UINT32 val;
	int invalid_rx2=0,invalid_rx3=0, invalid_rx4=0;
	int invalid_bias2=0, invalid_bias3=0, invalid_bias4=0;
	int stackId = PORT_TO_STACK_ID(port);

	uprintf("\n\n Chan Rx Power #1  Rx Power #2    Rx Power #3    Rx Power #4    \n");
	uprintf("+----+-----------+--------------+--------------+---------------+\n");

	if (port_optic->rx_power_reg == 0xffff || port_optic->rx_power_reg == 0x0)
	{
		uprintf("       N/A    ");
		invalid_rx1=1;
	}
	else if (mw2dbm(port_optic->rx_power_reg, &val))
		uprintf("    -%03d.%04d dBm",  val/10000, val%10000);
	else
		uprintf("     %03d.%04d dBm",  val/10000, val%10000);

	if (port_optic->rx2_power_reg == 0xffff || port_optic->rx2_power_reg == 0x0)
	{
		uprintf("       N/A    ");
		invalid_rx2=1;
	}
	else if (mw2dbm(port_optic->rx2_power_reg, &val))
		uprintf("  -%03d.%04d dBm",  val/10000, val%10000);
	else
		uprintf("   %03d.%04d dBm",  val/10000, val%10000);

	if (port_optic->rx3_power_reg == 0xffff || port_optic->rx3_power_reg == 0x0)
	{
		uprintf("       N/A    ");
		invalid_rx3=1;
	}
	else if (mw2dbm(port_optic->rx3_power_reg, &val))
		uprintf("  -%03d.%04d dBm",  val/10000, val%10000);
	else
		uprintf("   %03d.%04d dBm",  val/10000, val%10000);

	if (port_optic->rx4_power_reg == 0xffff || port_optic->rx4_power_reg == 0x0)
	{
		uprintf("       N/A    ");
		invalid_rx4=1;
	}
	else if (mw2dbm(port_optic->rx4_power_reg, &val))
		uprintf("  -%03d.%04d dBm",  val/10000, val%10000);
	else
		uprintf("   %03d.%04d dBm",  val/10000, val%10000);

	uprintf("\n");
	if (invalid_rx1)
		uprintf("                 ");
	else
	if (port_optic->rx_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_alarm)
		uprintf("        High-Alarm ");
	else
	if (port_optic->rx_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_warn)
		uprintf("        High-Warn  ");
	else
	if (port_optic->rx_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_alarm)
		uprintf("        Low-Alarm  ");	
	else
	if (port_optic->rx_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_warn)
		uprintf("        Low-Warn   ");
	else
		uprintf("        Normal     ");

	if (invalid_rx2)
		uprintf("               ");
	else
	if (port_optic->rx2_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_alarm)
		uprintf("    High-Alarm ");
	else
	if (port_optic->rx2_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_warn)
		uprintf("    High-Warn  ");
	else
	if (port_optic->rx2_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_alarm)
		uprintf("    Low-Alarm  ");	
	else
	if (port_optic->rx2_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_warn)
		uprintf("    Low-Warn   ");
	else
		uprintf("    Normal     ");

	if (invalid_rx3)
		uprintf("               ");
	else
	if (port_optic->rx3_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_alarm)
		uprintf("    High-Alarm ");
	else
	if (port_optic->rx3_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_warn)
		uprintf("    High-Warn  ");
	else
	if (port_optic->rx3_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_alarm)
		uprintf("    Low-Alarm  ");	
	else
	if (port_optic->rx3_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_warn)
		uprintf("    Low-Warn   ");
	else
		uprintf("    Normal     ");

	if (invalid_rx4)
		uprintf("               ");
	else
	if (port_optic->rx4_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_alarm)
		uprintf("    High-Alarm ");
	else
	if (port_optic->rx4_power_reg > sfp_monitor[stackId][index].thresholds.rx_power_high_warn)
		uprintf("    High-Warn  ");
	else
	if (port_optic->rx4_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_alarm)
		uprintf("    Low-Alarm  ");	
	else
	if (port_optic->rx4_power_reg < sfp_monitor[stackId][index].thresholds.rx_power_low_warn)
		uprintf("    Low-Warn   ");
	else
		uprintf("    Normal     ");

	uprintf("\n\n Chan  Tx Bias #1   Tx Bias #2     Tx Bias #3     Tx Bias #4    \n");
	uprintf("+----+-----------+--------------+--------------+---------------+\n");
	if (port_optic->tx_bias_current_reg == 0xffff || port_optic->tx_bias_current_reg == 0x0)
	{
		uprintf("       N/A  ");
		invalid_bias1=1;
	}
	else
		uprintf("     %3d.%03d mA",  port_optic->tx_bias_current_reg/500, (port_optic->tx_bias_current_reg*2)%1000);

	if (port_optic->tx2_bias_reg == 0xffff || port_optic->tx2_bias_reg == 0x0)
	{
		uprintf("       N/A  ");
		invalid_bias2=1;
	}
	else
		uprintf("     %3d.%03d mA",  port_optic->tx2_bias_reg/500, (port_optic->tx2_bias_reg*2)%1000);

	if (port_optic->tx3_bias_reg == 0xffff || port_optic->tx3_bias_reg == 0x0)
	{
		uprintf("       N/A  ");
		invalid_bias3=1;
	}
	else
		uprintf("     %3d.%03d mA",  port_optic->tx3_bias_reg/500, (port_optic->tx3_bias_reg*2)%1000);

	if (port_optic->tx4_bias_reg == 0xffff || port_optic->tx4_bias_reg == 0x0)
	{
		uprintf("       N/A  ");
		invalid_bias4=1;
	}
	else
		uprintf("     %3d.%03d mA",  port_optic->tx4_bias_reg/500, (port_optic->tx4_bias_reg*2)%1000);

		uprintf("\n");
	if (invalid_bias1)
		uprintf("                ");
	else
	if (port_optic->tx_bias_current_reg > sfp_monitor[stackId][index].thresholds.bias_high_alarm)
		uprintf("        High-Alarm");
	else
	if (port_optic->tx_bias_current_reg > sfp_monitor[stackId][index].thresholds.bias_high_warn)
		uprintf("        High-Warn ");
	else
	if (port_optic->tx_bias_current_reg < sfp_monitor[stackId][index].thresholds.bias_low_alarm)
		uprintf("        Low-Alarm ");
	else
	if (port_optic->tx_bias_current_reg < sfp_monitor[stackId][index].thresholds.bias_low_warn)
		uprintf("        Low-Warn  ");
	else
		uprintf("        Normal     ");

	if (invalid_bias2)
		uprintf("               ");
	else
	if (port_optic->tx2_bias_reg > sfp_monitor[stackId][index].thresholds.bias_high_alarm)
		uprintf("    High-Alarm");
	else
	if (port_optic->tx2_bias_reg > sfp_monitor[stackId][index].thresholds.bias_high_warn)
		uprintf("    High-Warn ");
	else
	if (port_optic->tx2_bias_reg < sfp_monitor[stackId][index].thresholds.bias_low_alarm)
		uprintf("    Low-Alarm ");
	else
	if (port_optic->tx2_bias_reg < sfp_monitor[stackId][index].thresholds.bias_low_warn)
		uprintf("    Low-Warn  ");
	else
		uprintf("    Normal     ");

	if (invalid_bias3)
		uprintf("               ");
	else
	if (port_optic->tx3_bias_reg > sfp_monitor[stackId][index].thresholds.bias_high_alarm)
		uprintf("    High-Alarm");
	else
	if (port_optic->tx3_bias_reg > sfp_monitor[stackId][index].thresholds.bias_high_warn)
		uprintf("    High-Warn ");
	else
	if (port_optic->tx3_bias_reg < sfp_monitor[stackId][index].thresholds.bias_low_alarm)
		uprintf("    Low-Alarm ");
	else
	if (port_optic->tx3_bias_reg < sfp_monitor[stackId][index].thresholds.bias_low_warn)
		uprintf("    Low-Warn  ");
	else
		uprintf("    Normal     ");

	if (invalid_bias4)
		uprintf("               ");
	else
	if (port_optic->tx4_bias_reg > sfp_monitor[stackId][index].thresholds.bias_high_alarm)
		uprintf("    High-Alarm");
	else
	if (port_optic->tx4_bias_reg > sfp_monitor[stackId][index].thresholds.bias_high_warn)
		uprintf("    High-Warn ");
	else
	if (port_optic->tx4_bias_reg < sfp_monitor[stackId][index].thresholds.bias_low_alarm)
		uprintf("    Low-Alarm ");
	else
	if (port_optic->tx4_bias_reg < sfp_monitor[stackId][index].thresholds.bias_low_warn)
		uprintf("    Low-Warn  ");
	else
		uprintf("    Normal     ");

	uprintf("\n\n");
}

static int 
sfp_monitor_threshold_init(int index, PORT_ID port)
{
  UINT8 message[64];
  int error_flag=0;
  int stackId = PORT_TO_STACK_ID(port);

  if (is_qsfp_optic_dom_supported(port))
  {
    if (qsfp_media_page_read(port, 128, 3, 64, (UINT8 *)&message[0]) != 0)
      error_flag = 1;
    else {
      memcpy((UINT8 *)&sfp_monitor[stackId][index].thresholds.temp_high_alarm,
             (UINT8 *)&message[0], 8);
      memcpy((UINT8 *)&sfp_monitor[stackId][index].thresholds.voltage_high_alarm,
             (UINT8 *)&message[16], 8);
      memcpy((UINT8 *)&sfp_monitor[stackId][index].thresholds.rx_power_high_alarm,
             (UINT8 *)&message[48], 8);
      memcpy((UINT8 *)&sfp_monitor[stackId][index].thresholds.bias_high_alarm,
             (UINT8 *)&message[56], 8);
    }
  } else {

    if(SPTR_PORT_DB(port)->platform_port.is_fluffy_present == 1) // optic present is checked in calling function
      if(hal_fluffy_select_ext_lrm_optic(port, FLUFFY_EEPROM_LRM_EXTERNAL) != LINK_OK)
        return CU_ERROR;

    if(sfp_media_read(port, SFP_EEPROM_ADDR_1, 
                               SFP_MONITOR_THRESHOLD_OFFSET, 
                               SFP_MONITOR_THRESHOLD_LEN,
                               (UINT8 *)&sfp_monitor[stackId][index].thresholds) 
                                  != SFP_MONITOR_THRESHOLD_LEN)
        error_flag = 1;

    if(SPTR_PORT_DB(port)->platform_port.is_fluffy_present == 1) 
      if(hal_fluffy_select_ext_lrm_optic(port, FLUFFY_EEPROM_LRM_INTERNAL) != LINK_OK)
        return CU_ERROR;

    if (error_flag == 1) {
      if (debugGlobal.system.optics)
        uprintf("OPTICAL MONITORING: THRESHOLDS READ FAILED, port %p\n", port);

      if (IS_PORT_UP(port) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set
        || (SPTR_PORT_DB(port))->port_config.dom_down_port_set) {
        ksprintf(message, "OPTICAL MONITORING: THRESHOLDS READ FAILED, port %p\n", port);
        send_port_optical_msg_type(message, OPTICAL_MONITORING_MSG_TYPE);
      }
      return -1;
    }
    return 0;
  }
}


static int
is_sfpp_capable_of_optical_monitoring (PORT_ID module_port_index)
{
  UINT8 vendor[SFP_VENDOR_NAME_LEN + 1];

  memset (&vendor[0], 0x0, SFP_VENDOR_NAME_LEN + 1);

  sfp_get_vendor_name (module_port_index, vendor);

  if (strncmp(vendor, "FOUNDRY NETWORKS", SFP_VENDOR_NAME_LEN) &&
      strncmp(vendor, "FOUNDRY-NETWORKS", SFP_VENDOR_NAME_LEN) &&
     strncmp(vendor, "Foundry Networks", SFP_VENDOR_NAME_LEN) &&
     strncmp(vendor, "RUCKUS", 6) && strncmp(vendor, "Ruckus", 6) &&
	  strncmp(vendor, "BROCADE", 7))
  {
	  if(dm_debug_mask & DM_OPTICAL_MONITOR_QA)
      	uprintf("is_sfpp_capable_of_optical_monitoring() vendor name=%s\n", &vendor[0]);
	  
	  if(IS_PORT_UP(module_port_index) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA))
	      optical_monitior_log_msg (NOT_FOUNDRY_OPTICS_MSG_TYPE, module_port_index);
      return 0;
  }

  return 1;
}

static void 
sfp_monitor_data_init(PORT_ID lport)
{
	UINT8 optic_data[6] = {0};
	enum SFP_MONITOR_DEVICE_TYPE sfp_type_flag = 0;
	int index=0;
	int found=0;
	int stackId = PORT_TO_STACK_ID(lport);

	if (SPTR_PORT_DB(lport)->port_oper_info.media_type == LINK_MEDIA_10GBASE_COPPER)
		return;

	if (SPTR_PORT_DB(lport)->port_oper_info.media_type == LINK_MEDIA_1GBASE_ACTIVE_CABLE)
		return;

	if (SPTR_PORT_DB(lport)->port_mtype == X10GIG_FIBER_PORT) 
	{
		if(!dom_non_brocade_on_off_set && !(SPTR_PORT_DB(lport))->port_config.dom_non_brocade_set) // if non-brocade support is not set, check; down-ports check in hal_optical_monitor()
		if(!is_sfpp_capable_of_optical_monitoring(lport))
		{
			uprintf("port %p is not capable of digital optical monitoring.\n", lport); 
			return;
		}
	} 
	else if(SPTR_PORT_DB(lport)->port_mtype == GIG_FIBER_PORT) 
	{
		if(!dom_non_brocade_on_off_set && !(SPTR_PORT_DB(lport))->port_config.dom_non_brocade_set) // if non-brocade support, 
		sfp_type_flag = is_sfp_capable_of_optical_monitoring(lport);
		if(sfp_type_flag != SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE)
			goto sfp_not_monitorable;
	}

	if(SPTR_PORT_DB(lport)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER) {
		if(is_fluffy_ext_lrm_optics_present(lport) != LINK_OK) // external optic does not exist
			return;
	}

	SPTR_PORT_DB(lport)->port_oper_info.dom_capable = 1;

  index = get_sfp_monitor_from_port(lport);
  if(debugGlobal.system.optics || debugGlobal.system.mibdom)
    uprintf("SFP/SFP+/QSFP+ port %p assigned sfp_monitor index=%d media type=%xx\n", lport, 
    index, SPTR_PORT_DB(lport)->port_oper_info.media_type);

	if (sfp_monitor[stackId][index].Optical_monitor_use != OPTICS_IN_MONITOR)
	{
		sfp_monitor[stackId][index].Optical_monitor_use = OPTICS_IN_MONITOR;
		sfp_monitor[stackId][index].inited = SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE;
		sfp_monitor[stackId][index].sfp_port = lport;
		sfp_monitor[stackId][index].enable0 = 0xFFC00000;
		sfp_monitor[stackId][index].enable1 = 0xFFC00000;
		sfp_monitor[stackId][index].flag0 = 0;
		sfp_monitor[stackId][index].flag1 = 0;
		sfp_monitor_set_interval(index, lport);
		sfp_monitor_threshold_init(index, lport);

		if (debugGlobal.system.mibdom)
			kprintf("%s: interval %d, port %p: done.\n", __FUNCTION__,
					sfp_monitor[stackId][index].interval, lport);
	}

sfp_not_monitorable:
	if(IS_PORT_UP(lport) || (dm_debug_mask & DM_OPTICAL_MONITOR_QA) || dom_down_port_on_off_set ||
		(SPTR_PORT_DB(lport))->port_config.dom_down_port_set)
	{
		if (sfp_type_flag == SFP_MONITOR_DEVICE_TYPE_NOT_FOUNDRY_QUALIFIED)
			optical_monitior_log_msg(NOT_FOUNDRY_OPTICS_MSG_TYPE, lport);

		else if(sfp_type_flag == SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE)
			optical_monitior_log_msg(OPTICAL_MONITORING_FOUNDRY_OPTICS_NOT_CAPABLE_MSG_TYPE, lport);
	}

	return;
}

static int 
get_dom_config_interval(int stackId, PORT_ID port, DOM_STACK_CONFIG *dom_conf)
{
  PORT_ID stack_port = PORT_TO_STACK_PORT(port) ;

  return (dom_conf[stack_port].interval);
}

void
cu_set_stack_dom_ipc_callback(int stk_id, pp_stack_dom_ipc_header *pp_stack_dom_header)
{
  PORT_ID port,  i;
  int max_num_sfp_monitor = get_num_sfp_monitors(stk_id);

  if (debugGlobal.system.mibdom)
    kprintf("gtime %d start cu_set_stack_dom_ipc_callback() - Sent PP_IPC_TYPE_SET_STACK_DOM to unit %u\n",
      g_time_counter, stk_id);

  if (!IS_CHASSIS_STACK_EXIST(stk_id))
  {
    debug_uprintf("%s:%d: : invalid stk_id %d\n", __FILE__, __LINE__, stk_id);
    return;
  }

#ifdef __PORT_EXTENSION__ 
  if (!STACK_AM_I_SLAVE && !STACK_AM_I_PE)
  {
    //debug_uprintf("%s:%d: master should not call\n",  __FILE__, __LINE__);
    return;
  }
#else
  if (!STACK_AM_I_SLAVE)
    return;
#endif

  for (i = 0; i < max_num_sfp_monitor; i++) 
  {
    int idx = get_dom_config_from_sfp_monitor(i);
    dom_config[stk_id][idx] = pp_stack_dom_header->dom_config[i];
    port = dom_config[stk_id][idx].port_id;
		
    if (port == BAD_PORT_ID) continue;

    port = ipc_port_to_sw_port(port);
    if (!IS_PORT_DB_VALID(port)) 
    {
      debug_uprintf("%s:%d: invalid port %p\n", __FILE__, __LINE__, port);
      continue;
    }

//		if((port == 26) || (port == 28) || (port == 30) ||(port == 266) || (port == 268) || (port == 270))
//			kprintf("cu_set_stack_dom_ipc_callback() port %p interval = %d dom_config[i].interval = %d\n",
//			  port, (SPTR_PORT_DB(port))->port_config.optical_monitor_interval, pp_stack_dom_header->dom_config[i].interval);


/*TODO: Indra : Here the slave and PE unit gets the callback and PORT_DB of those units get updated accordingly. Then the DOM starts in local unit.*/
    (SPTR_PORT_DB(port))->port_config.optical_monitor_interval = pp_stack_dom_header->dom_config[i].interval;
    (SPTR_PORT_DB(port))->port_config.dom_down_port_set   = pp_stack_dom_header->dom_config[i].dom_down_port_set;
    (SPTR_PORT_DB(port))->port_config.dom_non_brocade_set = pp_stack_dom_header->dom_config[i].dom_non_brocade_set;

    cu_optic_mon_per_port_zap_data(port);
  }

  if (debugGlobal.system.mibdom)
    kprintf("gtime %d end   cu_set_stack_dom_ipc_callback() - Sent PP_IPC_TYPE_SET_STACK_DOM to unit %u\n",
      g_time_counter, stk_id);
}

/* called in member unit */
static void 
sfp_get_one_port_optic_power_from_member(pp_stack_port_optic_ipc_header *port_optic)
{
	UINT8 media_type = 0;
	PORT_ID port = port_optic->port_id;
	int stackId = PORT_TO_STACK_ID(port);

#ifdef __PORT_EXTENSION__
	if(stackId != MY_BOOTUP_STACK_OR_PE_ID)
	{
		debug_uprintf("%s:%d: port %p not local\n", __FILE__, __LINE__, port);
		return;
	}

	port = ipc_port_to_sw_port(port);
	if (!IS_PORT_DB_VALID(port))
	{
		debug_uprintf("%s:%d: invalid port %p\n", __FILE__, __LINE__, port);
		return;
	}
#else
	if (!IS_PORT_DB_VALID(port))
		return;

	if( stackId != MY_BOOTUP_STACK_ID)
		return;
#endif

	media_type = SPTR_PORT_DB(port)->port_oper_info.media_type;

	if((dm_debug_mask & DM_OPTICAL_MONITOR_QA) ||debugGlobal.system.mibdom)
		uprintf("sfp_get_one_port_optic_power_from_member: Port %p, media type %d \n", 
				port, media_type); 

  if(SPTR_PORT_DB(port)->port_mtype == X10GIG_FIBER_PORT || SPTR_PORT_DB(port)->port_mtype == GIG_FIBER_PORT) 
  { 
    stack_member_sfpp_sfp_get_measure(port, port_optic);
    goto sfpp_debug;
  }

	sfp_get_temperature(port, (signed short *)(&port_optic->temp_reg));
	sfp_get_voltage(port, (UINT16 *)(&port_optic->voltage));
	sfp_get_tx_bias_current(port, (UINT16 *)(&port_optic->tx_bias_current_reg));
	sfp_get_rx_power(port, (UINT16 *)(&port_optic->rx_power_reg));
	sfp_get_thresholds(port, (void *)(&port_optic->sfp_thresholds));

	if (!is_qsfp_optic_dom_supported(port))
	{
		sfp_get_tx_power(port, (unsigned short *)(&port_optic->tx_power_reg));
	}	
	else {
		qsfp_get_volt(port, (UINT16 *)(&port_optic->voltage));
		qsfp_get_rx2to4_power(port, (unsigned short *)(&port_optic->rx2_power_reg));
		qsfp_get_tx2to4_bias(port, (unsigned short *)(&port_optic->tx2_bias_reg));
	}

sfpp_debug:
	if (debugGlobal.system.mibdom)
		debug_calibration_print(port, (UINT32)port_optic->temp_reg, (UINT16)port_optic->tx_bias_current_reg, (UINT16)port_optic->rx_power_reg);

	return;
}

void 
cu_show_stack_port_sfp_optic_ipc_callback_from_passive_unit(int stk_id, 
                                         PORT_ID port, UINT8  msg_type)
{
  UINT32 record_size = sizeof(pp_stack_port_optic_ipc_header);
  pp_stack_port_optic_ipc_header *pp_stack_port_optic_header = NULL;
  PORT_ID stack_port=0x0;

  if (debugGlobal.system.mibdom)
    kprintf("\nstart recv callback_from_passive msg %s unit %d, len=%u, port=%p type=%u\n", 
      (msg_type==PP_PORT_SHOW_SFP_OPTIC_ON_STACK_PORT)?"dom":"threshold",stk_id, chassis_operation_data_length, port, msg_type);

  if (!IS_CHASSIS_STACK_EXIST(stk_id))
    return;

#ifdef __PORT_EXTENSION__ 
  if (!STACK_AM_I_SLAVE && !STACK_AM_I_PE)
  {
    // debug_uprintf("%s:%d: master should not call\n", __FILE__, __LINE__);
    return;
  }
  // take care of IPC PE port but don't over-write IPC PE "port" such as 19/2/1 47/2/1 since the "port" is used to send back to Active
  stack_port = ipc_port_to_sw_port(port); 
#else
  if (!STACK_AM_I_SLAVE)
    return;

  stack_port = port; // take care regular stacking that check LRM adapter ports, regular stacking, these 2 ports are same
#endif

  pp_stack_port_optic_header = (pp_stack_port_optic_ipc_header *)dy_malloc(record_size);

  if (pp_stack_port_optic_header == NULL)
    return;

  pp_stack_port_optic_header->type = msg_type;
  pp_stack_port_optic_header->stackId = stk_id;
  pp_stack_port_optic_header->port_id = port; // IPC PE port or stacking port

  if(SPTR_PORT_DB(stack_port)->platform_port.is_fluffy_present == 1) {
    if(is_fluffy_ext_lrm_optics_present(stack_port) != LINK_OK)  // external optic does not exist
      pp_stack_port_optic_header->temp = 0xFFFE;
  }

  sfp_get_one_port_optic_power_from_member(pp_stack_port_optic_header );

  rel_ipc_send_msg(STACK_MASTER, REL_IPC_CHAN_BASE, 
                   IPC_MSGTYPE_CHASSIS_OPERATION, pp_stack_port_optic_header, record_size);

  rel_ipc_flush(STACK_MASTER, REL_IPC_CHAN_BASE); 

  if (debugGlobal.system.mibdom)
    kprintf("\nend   sent callback_from_passive msg %s unit %u, len=%u, port=%p, stacking_port=%p type=%u\n", 
      (msg_type==PP_PORT_SHOW_SFP_OPTIC_ON_STACK_PORT)?"dom":"threshold",stk_id, record_size, port, stack_port, msg_type);
  dy_free(pp_stack_port_optic_header);

}

void 
cu_show_stack_port_sfp_optic_ipc_callback_from_active_unit(int stk_id,   
                pp_stack_port_optic_ipc_header  *pp_stack_port_optic_header, UINT8 msg_type)
{
  int index =0;

  if (debugGlobal.system.mibdom)
    kprintf("\nstart recv ipc_callback_from_active msg %s unit %d, len=%u port=%p\n",
      (msg_type==PP_PORT_SEND_SFP_OPTIC_TO_STACK_MASTER)?"dom":"threshold", stk_id, chassis_operation_data_length,pp_stack_port_optic_header->port_id);

  if (!IS_CHASSIS_STACK_EXIST(stk_id))
    return;

  if (STACK_AM_I_MEMBER)	
    return;

  if (!IS_PORT_DB_VALID(pp_stack_port_optic_header->port_id))
    return;

  sfp_port_optic_into.temp_reg = pp_stack_port_optic_header->temp_reg;
  sfp_port_optic_into.voltage = pp_stack_port_optic_header->voltage;
  sfp_port_optic_into.tx_bias_current_reg = pp_stack_port_optic_header->tx_bias_current_reg;
  sfp_port_optic_into.tx_power_reg = pp_stack_port_optic_header->tx_power_reg;
  sfp_port_optic_into.rx_power_reg = pp_stack_port_optic_header->rx_power_reg;

  sfp_port_optic_into.rx2_power_reg = pp_stack_port_optic_header->rx2_power_reg;

  sfp_port_optic_into.rx3_power_reg = pp_stack_port_optic_header->rx3_power_reg;
  sfp_port_optic_into.rx4_power_reg = pp_stack_port_optic_header->rx4_power_reg;
  sfp_port_optic_into.tx2_bias_reg = pp_stack_port_optic_header->tx2_bias_reg;
  sfp_port_optic_into.tx3_bias_reg = pp_stack_port_optic_header->tx3_bias_reg;
  sfp_port_optic_into.tx4_bias_reg = pp_stack_port_optic_header->tx4_bias_reg;

  memcpy((void *)&sfp_port_optic_into.thresholds,  
           (void *)&pp_stack_port_optic_header->sfp_thresholds,
           sizeof(SFP_MONITOR_THRESHOLDS));

  if(debugGlobal.system.mibdom) 
    debug_calibration_print(pp_stack_port_optic_header->port_id, (UINT32)pp_stack_port_optic_header->temp_reg, 
    (UINT16)pp_stack_port_optic_header->tx_bias_current_reg, (UINT16)pp_stack_port_optic_header->rx_power_reg);


  index = get_port_sfp_monitor_index(pp_stack_port_optic_header->port_id);
  if (index == -1) 
  {
    debug_uprintf("%s:%d: can not found sfp_monitor for port %p\n",  __FILE__, __LINE__,
                       pp_stack_port_optic_header->port_id);

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: %s:%d: can not found sfp_monitor for port %p\n",  __FILE__, __LINE__,
                       pp_stack_port_optic_header->port_id);
    return;
  }

  memcpy((void *)&sfp_monitor[stk_id][index].thresholds,  
         (void *)&sfp_port_optic_into.thresholds, sizeof(SFP_MONITOR_THRESHOLDS));

  if (msg_type == PP_PORT_SEND_SFP_OPTIC_TO_STACK_MASTER)
    hal_show_sfp_optic_temp_power(pp_stack_port_optic_header->port_id /*, 0 */);
  else if (msg_type == PP_PORT_SEND_SFP_OPTIC_TH_TO_STACK_MASTER)
    hal_sfp_monitor_thresholds_display(pp_stack_port_optic_header->port_id);

  if (debugGlobal.system.mibdom)
    kprintf("end   recv ipc_callback_from_active msg %s unit %d, len=%u port=%p\n",
      (msg_type==PP_PORT_SEND_SFP_OPTIC_TO_STACK_MASTER)?"dom":"threshold", stk_id, chassis_operation_data_length,pp_stack_port_optic_header->port_id);

	clear_parser_wait_until_callback();	/* process next command */
	
}

/* dom_config[stackId][i]: i is sfp_monitor index 
   so dom_config[stackId][i] targeting an sfp_monitor, instead of a port */
static unsigned int
get_dom_config_from_sfp_monitor(int sfp_monitor)
{
  return sfp_monitor;
}

static unsigned int
get_dom_config_from_port(PORT_ID port)
{
  int sfp_monitor = get_sfp_monitor_from_port(port);
  return get_dom_config_from_sfp_monitor(sfp_monitor);
}


/********************************************************/
/*** chassis registrations:  all platforms  ***/
/********************************************************/


static int 
get_num_sfp_monitors(int stackId)
{

  if (chassisStackIsSIDEWINDER(stackId)) 
    return icx7750_get_num_sfp_monitors(stackId);
  else if (chassisStackIsSpatha(stackId))
    return icx7450_get_num_sfp_monitors(stackId);
  else if (chassisStackIsSica(stackId))
    return icx72x0_get_num_sfp_monitors(stackId);
  else if (chassisStackIsMinions(stackId))
    return icx7150_get_num_sfp_monitors(stackId);

    return 0;
}


static PORT_ID 
get_port_from_sfp_monitor(int stack_id, int sfp_monitor)
{
  if (chassisStackIsSIDEWINDER(stack_id)) 
    return icx7750_get_port_from_sfp_monitor(stack_id, sfp_monitor);
  else if (chassisStackIsSpatha(stack_id))
    return icx7450_get_port_from_sfp_monitor(stack_id, sfp_monitor);
  else if(chassisStackIsSica(stack_id))
    return icx72x0_get_port_from_sfp_monitor(stack_id, sfp_monitor);
  else if (chassisStackIsMinions(stack_id))
    return icx7150_get_port_from_sfp_monitor(stack_id, sfp_monitor);

  return -1;
}

/* media read function for all sfp, sfp+, qsfp */
static int 
sfp_media_read(PORT_ID port, UINT8 addr, UINT8 offset, UINT32 length, UINT8 *buf)
{
	CHASSIS_PROFILE_DATA *cp = NULL;
	STACK_ID stack_id = PORT_TO_STACK_ID(port);
	int rc = 0;
	PORT_ID stack_port = PORT_TO_STACK_PORT(port);

	if (MY_BOOTUP_STACK_ID != stack_id) return -1;

	if(IS_SIDEWINDER() && (stack_port>=128)) {
			rc = icx7750_media_rear_qsfp_read(port, addr, offset, length, buf);
			return rc;
		}

	cp = GET_CHASSIS_PTR(stack_id);
	if (NULL == cp) {
		return -1;
	}

	rc = cp->media_mgr->media_read(port, addr, offset, length, buf);

	return rc;
}

static int 
qsfp_media_page_read(PORT_ID port, UINT8 addr, UINT8 page, UINT8 length, UINT8 *buf)
{
  if (IS_SIDEWINDER()) 
    return icx7750_qsfp_page_read(port, addr, page, length, buf);
  else if (IS_SPATHA())
    return spatha_qsfp_page_read(port, addr, page, length, buf);
  
  return 0;
}




static unsigned int
get_sfp_monitor_from_port(PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);

  if (chassisStackIsSIDEWINDER(stackId)) 
    return icx7750_get_sfp_monitor_from_port(port);
  else if (chassisStackIsSpatha(stackId))
    return icx7450_get_sfp_monitor_from_port(port);
  else if (chassisStackIsSica(stackId))
    return icx72x0_get_sfp_monitor_from_port(port);
  else if (chassisStackIsMinions(stackId))
    return icx7150_get_sfp_monitor_from_port(port);
  else if (chassisStackIsTanto(stackId))
    return icx7650_get_sfp_monitor_from_port(port);

  return -1;
}

int icx7750_optical_monitor_init(void *data)
  { 
    icx7750_om_init();
    icx7750_om_scheduler_init(&monitor);
    icx7750_om_scheduler_init(&service);

    return icx7750_dom_init();
  }

int icx7650_optical_monitor_init(void *data)
  { // TANTO_TODO_KIRAN: update this function for tanto

    return 0;
  }


int icx7450_optical_monitor_init(void *data)
  { 
    icx7450_om_init();
    icx7450_om_scheduler_init(&monitor);
    icx7450_om_scheduler_init(&service);

    return icx7450_dom_init();
  }


int icx7250_optical_monitor_init(void *data)
  {
    icx72x0_om_init();
    icx72x0_om_scheduler_init(&monitor);
    icx72x0_om_scheduler_init(&service);

    return icx72x0_dom_init();
  }

int 
hal_optical_monitor_init ()
{
        CHASSIS_PROFILE_DATA *cp = NULL;
        int rc = 0;

        cp = GET_CHASSIS_PTR(MY_BOOTUP_STACK_ID);
        if (NULL == cp) {
                return -1;
        }

  	om_scheduler_init();
        rc = cp->media_mgr->optical_monitor_init(cp->media_mgr);

        return rc;
}

/*** end of chassis registrations  ***/

/********************************************************/
/******** chassis dependent section:  sidewinder  *******/
/********************************************************/
static int 
icx7750_dom_init(void)
{
  extern UINT32 gi_board_type;

  OpticalMonitor_sche = 10; // choose a prime #
  OpticalMonitor_SFP_timer = 15; // choose a prime #
  icx7750_slot_serv = (MY_BOOTUP_STACK_ID-1)*4;
  icx7750_sl = (MY_BOOTUP_STACK_ID-1)*4;

  if (gi_board_type != SW_BOARD_TYPE_48XGC) 
    icx7750_set_rear_module_qsfp(); // set the rear module QSFP
  else
    is_icx7750_48c_rear_module_exist_with_qsfp(2);

  sv_OpticalMonitor_token = sv_set_timer (OpticalMonitor_sche*SECOND, 
                                          REPEAT_TIMER, hal_optical_monitor, 0);

  sv_OM_SFP_token = sv_set_timer (OpticalMonitor_SFP_timer*SECOND, 
                                   REPEAT_TIMER, hal_sfp_optical_monitoring_service, 0);

  return 0;
}


static int
icx7750_om_scheduler_init(struct om_sched *p_sched)
{
  int s = 0;
  int cid = chassisGetChassisId();

  p_sched->num_slots = 0;

  for (s = 0; s < MAX_LOCAL_SLOT; s++)
  {
    struct om_sched_slot *p_slot = &p_sched->slots[s];
    int module = MAKE_MODULE_ID(MY_BOOTUP_STACK_ID, s);

    if (!is_module_exist(module)) continue;
	if((s == 2) && (!is_sw_rear_module_present())) continue;

    p_sched->num_slots++;
    p_slot->slot = s;
    p_slot->enable = 1;

    switch (cid) {
    case SW_BOARD_TYPE_48XGF:
      if (s == 0)
      {
        p_slot->num_ports = 48;
        p_slot->num_ports_per_cycle = 48; /* wyang, temp change, DOM_PORT_MONITOR_PER_TIME; */
      }
      else if (s == 1)
      {
        p_slot->num_ports = 6*4;
        p_slot->num_ports_per_cycle = 6;
      }
      else if (s == 2)
      {
        p_slot->num_ports = 6*4;
        p_slot->num_ports_per_cycle = 6;
      }
      break;
    case SW_BOARD_TYPE_48XGC:
      if (s == 0)
      {
        p_slot->enable = 0;
      }
      else if (s == 1)
      {
        p_slot->num_ports = 6*4;
        p_slot->num_ports_per_cycle = 6;
      }
      else if (s == 2)
      {
        p_slot->num_ports = 6*4;
        p_slot->num_ports_per_cycle = 6;
      }
      break;
    case SW_BOARD_TYPE_20QXG:
      if (s == 0)
      {
        p_slot->num_ports = 8 + 12*4;
        p_slot->num_ports_per_cycle = DOM_PORT_MONITOR_PER_TIME;
      }
      else if (s == 1)
      {
        p_slot->num_ports = 6*4;
        p_slot->num_ports_per_cycle = 6;
      }
      else if (s == 2)
      {
        p_slot->num_ports = 6*4;
        p_slot->num_ports_per_cycle = 6;
      }
      break;
    default:
      break;
    }
  }
  return 0;
}

static void 
icx7750_om_init(void)
{
  return;

}

static int  
icx7750_get_num_sfp_monitors(int stackId)
{

#ifdef SIDEWINDER_BREAKOUT
  if (chassisStackIsSIDEWINDER_48XGF(stackId)) 
    return (48 + (6 + 6) * 4);
  else if (chassisStackIsSIDEWINDER_48XGC(stackId)) 
    return ((6 + 6) * 4);
  else if (chassisStackIsSIDEWINDER_32QXG(stackId))
    return ((8 + (12 + 6 + 6) * 4));
#else
  if (chassisStackIsSIDEWINDER_48XGF(stackId)) 
    return 60;
  else if (chassisStackIsSIDEWINDER_48XGC(stackId)) 
    return 12;
  else if (chassisStackIsSIDEWINDER_32QXG(stackId))
    return 32;
#endif  
  else
    return 0;

}


/*                 sfp_port<----> sfp_monitor mapping */
/* 48F:  module 0,       0 <----> 0
 *                       1 <----> 1
 *                       
 *                       47 <---->47
 *
 *       module 1        0 <----> 48
 *                       1 <----> 49
 *                       
 *                       5 <----> 53
 *
 *       module 2        0 <----> 54
 *                       1 <----> 55
 *                       
 *                       5 <----> 59
 *
 * 48C:  module 1,       0 <----> 0
 *                       1 <----> 1
 *                       
 *                       5 <---->5
 *
 *       module 2        0 <----> 6
 *                       1 <----> 7
 *                       
 *                       5 <----> 11
 *                       
 * 48C:  module 0,       0 <----> 0
 *                       1 <----> 1
 *                       
 *                      19 <---->19
 *
 *       module 1        0 <----> 20
 *                       1 <----> 21
 *                       
 *                       5 <----> 25
 *
 *       module 2        0 <----> 26
 *                       1 <----> 27
 *                       
 *                       5 <----> 3
 */

static PORT_ID 
icx7750_get_port_from_sfp_monitor(int stackId, int sfp_monitor)
{
  int local_module = 0, module_port = 0;
  int module = 0;

  if (chassisStackIsSIDEWINDER_48XGF(stackId)) 
  {
    if (sfp_monitor < 48) {local_module = 0; module_port = sfp_monitor;}
    else if (sfp_monitor < (48 + 6*4)) {local_module = 1; module_port = sfp_monitor - 48;}
    else if (sfp_monitor < (48 + 2*6*4)) {local_module = 2; module_port = sfp_monitor - (48 + 6*4);}
  } 
  else if (chassisStackIsSIDEWINDER_48XGC(stackId)) 
  {
    if (sfp_monitor < (6*4)) {local_module = 1; module_port = sfp_monitor;}
    else if (sfp_monitor < (2*6*4)) {local_module = 2; module_port = sfp_monitor - 6*4;}
  }
  else if (chassisStackIsSIDEWINDER_32QXG(stackId))
  {
    if (sfp_monitor < (8 + 12*4)) {local_module = 0; module_port = sfp_monitor;}
    else if (sfp_monitor < (8 + (6 + 12)*4)) {local_module = 1; module_port = sfp_monitor - (8 + 12*4);}
    else if (sfp_monitor < (8 + (2*6 + 12)*4)) {local_module = 2; module_port = sfp_monitor - (8 + (6 + 12)*4);}
  }
  else
  {
    OPTMON_DTRACE (("\nicx7750_get_port_from_sfp_monitor: "
                      "failed in getting a port for sfp_monitor %d\n", sfp_monitor));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: icx7750_get_port_from_sfp_monitor: "
                      "failed in getting a port for sfp_monitor %d\n", sfp_monitor);
  }

  module = MAKE_MODULE_ID(stackId, local_module);
  return MAKE_PORTID(module, module_port);
}

static unsigned int 
icx7750_get_sfp_monitor_from_port(PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);
  int stack_module = PORT_TO_MODULE_ID_LOCAL(port);
  int module_port = PORT_TO_MODULE_PORT(port);
  UINT32 moduleId = STACK_TO_BASE_MODULE(stackId);
  int mod0_ports = 0, mod1_ports = 0, mod2_ports = 0;
  extern int get_sidewinder_opt_stack_port();
  extern UINT32 gi_board_type;

  /* total number of ports on module 1 and 2 */
  /*
  mod0_ports = g_module[moduleId].number_of_ports; 
  mod1_ports = g_module[moduleId + 1].number_of_ports; 
  mod2_ports = g_module[moduleId + 2].number_of_ports; 
   */
  /* sfp_monitor is only for CB and not for PE. total number ports are known.
      * g_module[moduleId].number_of_ports are used by CB + PE.
      * module_port from above MACRO becomes 0,4,8,12,16,20 from breakout feature for 6 QSFPs.
      * It does not access logical sub-ports. Anyway, we only access physical QSFP once for sfp_monitor()
      * No need to access logical sub-ports to save CPUs. changing to module_port/4 to get the main port
      */
  if(gi_board_type == SW_BOARD_TYPE_20QXG)
    mod0_ports = MAX_ICX7750_SLOT1_QSFP;
  else
    mod0_ports = MAX_ICX7750_PHYSICAL_SFPP;
  mod1_ports = ICX7750_QSFP_SLOT2; 
  mod2_ports = ICX7750_QSFP_SLOT3;

  if (chassisStackIsSIDEWINDER_32QXG(stackId))
  {
//    if (stack_module == 0) return module_port; for breakout ports in 26Q
    if (stack_module == 0) return get_sidewinder_opt_stack_port(port);
    else if (stack_module == 1) return (mod0_ports + module_port/4);
    else if (stack_module == 2) return (mod0_ports + mod1_ports + module_port/4);
  }	
  else if (chassisStackIsSIDEWINDER_48XGF(stackId))
  {
    if (stack_module == 0) return module_port;
    else if (stack_module == 1) return (mod0_ports + module_port/4);
    else if (stack_module == 2) return (mod0_ports + mod1_ports + module_port/4);
  }
  else if (chassisStackIsSIDEWINDER_48XGC(stackId))
  {
    if (stack_module == 1) return module_port/4;
    else if (stack_module == 2) return (mod1_ports + module_port/4);
  }

  OPTMON_DTRACE (("\nicx7750_get_sfp_monitor_from_port - "
                     "port=%p: failed in getting a sfp_monitor\n", port));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nicx7750_get_sfp_monitor_from_port - "
                     "port=%p: failed in getting a sfp_monitor\n", port);
  return -1;
}
/*** end of sidewinder  ***/

/********************************************************/
/********** chassis dependent section:  spatha  *********/
/********************************************************/
static int 
icx7450_dom_init(void)
{

  OpticalMonitor_sche = 10; // choose a prime #
  OpticalMonitor_SFP_timer = 15; // choose a prime #

  sv_OpticalMonitor_token = sv_set_timer (OpticalMonitor_sche*SECOND, 
                                          REPEAT_TIMER, hal_optical_monitor, 0);

  sv_OM_SFP_token = sv_set_timer (OpticalMonitor_SFP_timer*SECOND, 
                                   REPEAT_TIMER, hal_sfp_optical_monitoring_service, 0);

  return 0;
}


static int
icx7450_om_scheduler_init(struct om_sched *p_sched)
{
  int s = 0;
  int cid = chassisGetChassisId();

  p_sched->num_slots = 0;

  for (s = 0; s < MAX_LOCAL_SLOT; s++)
  {
    struct om_sched_slot *p_slot = &p_sched->slots[s];
    int module = MAKE_MODULE_ID(MY_BOOTUP_STACK_ID, s);
    int module_type = 0;

    if (!is_module_exist(module)) continue;

    p_sched->num_slots++;
    p_slot->slot = s;
    p_slot->enable = 1;

    module_type = get_spatha_module_type(MY_BOOTUP_STACK_ID, s);

    switch (module_type) {

      case GS_SPATHA_STACK_BASE_24G:
      case GS_SPATHA_STACK_BASE_24P:
      case GS_SPATHA_STACK_BASE_48G:
      case GS_SPATHA_STACK_BASE_48P:
      case GS_SPATHA_STACK_BASE_32P:
      case GS_SPATHA_STACK_MOD_4XGC:
        p_slot->enable = 0;    
        break;

      case GS_SPATHA_STACK_BASE_48GF:
        p_slot->num_ports = 48;
        p_slot->num_ports_per_cycle = 12;
        break;

      case GS_SPATHA_STACK_MOD_4XGF:
      case GS_SPATHA_STACK_MOD_4GF:
        p_slot->num_ports = 4;
        p_slot->num_ports_per_cycle = 4;
        break;

      case GS_SPATHA_STACK_MOD_QXG:
        p_slot->num_ports = 1;
        p_slot->num_ports_per_cycle = 1;
        break;
      default:
        break;
    }
  }
  return 0;
}

static void 
icx7450_om_init(void)
{
  return;


#if 0
  for (module = 0; module < MAX_LOCAL_SLOT; module++)
  { 
    int module_type;

    if (!is_module_exist(module)) continue;

    module_type = get_spatha_module_type(module);

    switch (module_type) {
      case GS_SPATHA_STACK_BASE_48GF:
        max_num_sfp_monitor += 48;
        break;

      case GS_SPATHA_STACK_MOD_4XGF:
      case GS_SPATHA_STACK_MOD_4GF:
        max_num_sfp_monitor += 4;
        break;

      case GS_SPATHA_STACK_MOD_QXG:
        max_num_sfp_monitor += 1;
        break;
      default:
        break;
    }
  }
#endif

}

static int 
icx7450_get_num_sfp_monitors(int stackId)
{
#if 0
  int module = STACK_TO_MODULE_ID(stackId);
  int m = 0;
  int max_num_sfp_monitor = 0;

  for (m = 0; m < MAX_LOCAL_SLOT; m++)
  { 
    int module_type;

    if (!is_module_exist(module + m)) continue;

    module_type = get_spatha_module_type(stackId, module + m);

    switch (module_type) {
      case GS_SPATHA_STACK_BASE_48GF:
        max_num_sfp_monitor += 48;
        break;

      case GS_SPATHA_STACK_MOD_4XGF:
      case GS_SPATHA_STACK_MOD_4GF:
        max_num_sfp_monitor += 4;
        break;

      case GS_SPATHA_STACK_MOD_QXG:
        max_num_sfp_monitor += 1;
        break;
      default:
        break;
    }
  }
  return max_num_sfp_monitor;

#else
  if (chassisStackIsSpatha_24G(stackId) || 
      chassisStackIsSpatha_24P(stackId) ||
      chassisStackIsSpatha_32P(stackId) ||
      chassisStackIsSpatha_48G(stackId) ||
      chassisStackIsSpatha_48P(stackId)) 
    return 12;
  else if (chassisStackIsSpatha_48GF(stackId)) 
    return 60;
  else
    return 0;
#endif
}



/*              sfp_port <----> sfp_monitor mapping */
/* 24G/P:  module 1    0 <----> 0
 * 48G/P:              1 <----> 1
 *                     2 <----> 2
 *                     3 <----> 3
 *
 *       module 2      0 <----> 4
 *                     1 <----> 5
 *                     2 <----> 6
 *                     3 <----> 7
 *
 *       module 3      0 <----> 8
 *                     1 <----> 9
 *                     2 <----> 10
 *                     3 <----> 11
 *
 * 48F:  module 0,     0 <----> 0
 *                     1 <----> 1
 *                       
 *                    47 <----> 47
 *
 *       module 1      0 <----> 48
 *                     1 <----> 49
 *                     2 <----> 50
 *                     3 <----> 51
 *
 *       module 2      0 <----> 52
 *                     1 <----> 53
 *                     2 <----> 54
 *                     3 <----> 55
 *                     
 *       module 3      0 <----> 56
 *                     1 <----> 57
 *                     2 <----> 58
 *                     3 <----> 59
 */

static PORT_ID 
icx7450_get_port_from_sfp_monitor(int stackId, int sfp_monitor)
{
  int local_module = 0, module_port = 0;
  int module = 0;

  if (chassisStackIsSpatha_24G(stackId) || 
      chassisStackIsSpatha_24P(stackId) ||
      chassisStackIsSpatha_48G(stackId) ||
      chassisStackIsSpatha_48P(stackId) || 
      chassisStackIsSpatha_32P(stackId)) 
  {
    if (sfp_monitor < 4) {local_module = 1; module_port = sfp_monitor;}
    else if (sfp_monitor < 8) {local_module = 2; module_port = sfp_monitor - 4;}
    else if (sfp_monitor < 12) {local_module = 3; module_port = sfp_monitor - 8;}
  } 
  else if (chassisStackIsSpatha_48GF(stackId)) 
  {
    if (sfp_monitor < 48) {local_module = 0; module_port = sfp_monitor;}
    else if (sfp_monitor < 52) {local_module = 1; module_port = sfp_monitor - 48;}
    else if (sfp_monitor < 56) {local_module = 2; module_port = sfp_monitor - 52;}
    else if (sfp_monitor < 60) {local_module = 3; module_port = sfp_monitor - 56;}
  } 
  else
  {
    OPTMON_DTRACE (("\nicx7450_get_port_from_sfp_monitor: "
                      "failed in getting a port for sfp_monitor %d\n", sfp_monitor));

	fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nicx7450_get_port_from_sfp_monitor: "
                      "failed in getting a port for sfp_monitor %d\n", sfp_monitor);
  }

  module = MAKE_MODULE_ID(stackId, local_module);
  return MAKE_PORTID(module, module_port);
}

static unsigned int 
icx7450_get_sfp_monitor_from_port(PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);
  int stack_module = PORT_TO_MODULE_ID_LOCAL(port);
  int module_port = PORT_TO_MODULE_PORT(port);
  UINT32 moduleId = STACK_TO_BASE_MODULE(stackId);
  int mod0_ports = 0;

  /* total number of ports on module 0, 1, 2, 3 */
  //mod0_ports = g_module[moduleId].number_of_ports; 

  if (chassisStackIsSpatha_48GF(stackId))
  {
    if (stack_module == 0) return module_port;
    else if (stack_module == 1) return (48 + module_port);
    else if (stack_module == 2) return (48 + 4 + module_port);
    else if (stack_module == 3) return (48 + 4 + 4 + module_port);
  }
  else if (chassisStackIsSpatha_24G(stackId) ||
           chassisStackIsSpatha_24P(stackId) ||
	       chassisStackIsSpatha_48G(stackId) ||
           chassisStackIsSpatha_48P(stackId) ||
           chassisStackIsSpatha_32P(stackId))
  {
    if (stack_module == 1) return module_port;
    else if (stack_module == 2) return (4 + module_port);
    else if (stack_module == 3) return (4 + 4 + module_port);
  }

  OPTMON_DTRACE (("\nicx7450_get_sfp_monitor_from_port - "
                     "port=%p: failed in getting a sfp_monitor\n", port));

  fitrace(LINK_TRACE, FITRACE_LINK_OPTICS_OP,
				TRACE_INFO,  
				"INFO: \nicx7450_get_sfp_monitor_from_port - "
                     "port=%p: failed in getting a sfp_monitor\n", port);

  return -1;
}
/*** end of spatha  ***/

/********************************************************/
/********** chassis dependent section:  sica  *********/
/********************************************************/
static int 
icx72x0_dom_init(void)
{

  OpticalMonitor_sche = 10; // choose a prime #
  OpticalMonitor_SFP_timer = 15; // choose a prime #

  sv_OpticalMonitor_token = sv_set_timer (OpticalMonitor_sche*SECOND, 
                                          REPEAT_TIMER, hal_optical_monitor, 0);

  sv_OM_SFP_token = sv_set_timer (OpticalMonitor_SFP_timer*SECOND, 
                                   REPEAT_TIMER, hal_sfp_optical_monitoring_service, 0);

  return 0;
}


static int
icx72x0_om_scheduler_init(struct om_sched *p_sched)
{
  int s = 0;
  int cid = chassisGetChassisId();

  p_sched->num_slots = 0;

  for (s = 0; s < MAX_LOCAL_SLOT; s++)
  {
    struct om_sched_slot *p_slot = &p_sched->slots[s];
    int module = MAKE_MODULE_ID(MY_BOOTUP_STACK_ID, s);
    int module_type = 0;

    if (!is_module_exist(module)) continue;

    p_sched->num_slots++;
    p_slot->slot = s;
    p_slot->enable = 1;

    module_type = get_sica_module_type(MY_BOOTUP_STACK_ID, s);

    switch (module_type) {

      case GS_SICA_L_STACK_BASE_24G:
	  case GS_SICA_STACK_BASE_24G:
	  case GS_SICA_STACK_BASE_48G:
	  case GS_SICA_STACK_BASE_24P:
	  case GS_SICA_STACK_BASE_48P:
        p_slot->enable = 0;    
        break;

      case GS_SICA_L_STACK_MOD_4GF:
        p_slot->num_ports = 4;
        p_slot->num_ports_per_cycle = 4;
        break;

      case GS_SICA_STACK_MOD_8XGF:
        p_slot->num_ports = 8;
        p_slot->num_ports_per_cycle = 8;
        break;

      default:
        break;
    }
  }
  return 0;
}

static void 
icx72x0_om_init(void)
{
  return;
}

static int 
icx72x0_get_num_sfp_monitors(int stackId)
{
  if (chassisStackIsSica_24G(stackId) || 
      chassisStackIsSica_24P(stackId) ||
      chassisStackIsSica_48G(stackId) ||
      chassisStackIsSica_48P(stackId)) 
    return 8;
  else if (chassisStackIsSicaL_24G(stackId)) 
    return 4;
  else
    return 0;
}


static PORT_ID 
icx72x0_get_port_from_sfp_monitor(int stackId, int sfp_monitor)
{
  int local_module = 0, module_port = 0;
  int module = 0;

  local_module = 1; 
  module_port = sfp_monitor;

  module = MAKE_MODULE_ID(stackId, local_module);
  return MAKE_PORTID(module, module_port);
}

static unsigned int 
icx72x0_get_sfp_monitor_from_port(PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);
  int stack_module = PORT_TO_MODULE_ID_LOCAL(port);
  int module_port = PORT_TO_MODULE_PORT(port);

  return module_port;
}

/*
  * Minions DOM Initialization 2016-July
  */
static int 
icx7150_dom_init(void)
{

  OpticalMonitor_sche = 10; // choose a prime #
  OpticalMonitor_SFP_timer = 15; // choose a prime #

  sv_OpticalMonitor_token = sv_set_timer (OpticalMonitor_sche*SECOND, 
                                          REPEAT_TIMER, hal_optical_monitor, 0);

  sv_OM_SFP_token = sv_set_timer (OpticalMonitor_SFP_timer*SECOND, 
                                   REPEAT_TIMER, hal_sfp_optical_monitoring_service, 0);

  return 0;
}


static int
icx7150_om_scheduler_init(struct om_sched *p_sched)
{
  int s = 0;
  int cid = chassisGetChassisId();

  p_sched->num_slots = 0;

  for (s = 0; s < MAX_LOCAL_SLOT; s++)
  {
    struct om_sched_slot *p_slot = &p_sched->slots[s];
    int module = MAKE_MODULE_ID(MY_BOOTUP_STACK_ID, s);
    int module_type = 0;

    if (!is_module_exist(module)) continue;

    p_sched->num_slots++;
    p_slot->slot = s;
    p_slot->enable = 1;

    module_type = get_minions_module_type(MY_BOOTUP_STACK_ID, s);

    switch (module_type) {

      case GS_MINIONS_STACK_BASE_24G:
      case GS_MINIONS_STACK_BASE_C12P:
      case GS_MINIONS_STACK_BASE_24P:
      case GS_MINIONS_STACK_BASE_48G:
      case GS_MINIONS_STACK_BASE_48P:
      case GS_MINIONS_STACK_BASE_48PF:
      case GS_MINIONS_STACK_BASE_48ZP:
      case GS_MINIONS_STACK_MOD_2GC:
        p_slot->enable = 0;    
        break;

      case GS_MINIONS_STACK_MOD_8XGF:
        p_slot->num_ports = 8;
        p_slot->num_ports_per_cycle = 8;
        break;

      case GS_MINIONS_STACK_MOD_4XGF:
        p_slot->num_ports = 4;
        p_slot->num_ports_per_cycle = 4;
        break;

      case GS_MINIONS_STACK_MOD_2XGF:
        p_slot->num_ports = 2;
        p_slot->num_ports_per_cycle = 2;
        break;

      default:
        break;
    }
  }
  return 0;
}

static int 
icx7150_get_num_sfp_monitors(int stackId)
{
  if (chassisStackIsMinions_24G(stackId) || 
      chassisStackIsMinions_24P(stackId) ||
      chassisStackIsMinions_48G(stackId) ||
      chassisStackIsMinions_48P(stackId) ||
      chassisStackIsMinions_48PF(stackId)) 
    return 4;
  else if (chassisStackIsMinions_C12P(stackId)) 
    return 2;
  else if (chassisStackIsMinions_48ZP(stackId)) 
    return 8;
  else
    return 0;
}


static PORT_ID 
icx7150_get_port_from_sfp_monitor(int stackId, int sfp_monitor)
{
  int local_module = 0, module_port = 0;
  int module = 0;

  local_module = 1; 
  module_port = sfp_monitor;

  module = MAKE_MODULE_ID(stackId, local_module);
  return MAKE_PORTID(module, module_port);
}

static unsigned int 
icx7150_get_sfp_monitor_from_port(PORT_ID port)
{
  int stackId = PORT_TO_STACK_ID(port);
  int stack_module = PORT_TO_MODULE_ID_LOCAL(port);
  int module_port = PORT_TO_MODULE_PORT(port);

  return module_port;
}

int 
icx7150_optical_monitor_init(void *data)
{ 
  icx7150_om_scheduler_init(&monitor);
  icx7150_om_scheduler_init(&service);

  return icx7150_dom_init();
}  

static int
sfpp_sfp_get_measure(PORT_ID port, qsfpp_port_optic_t *port_optic)
{
  SFP_CALIBRATE_DATA cal_data;
  int bytes = 0;

  if(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER)
    if(hal_fluffy_select_ext_lrm_optic(port, FLUFFY_EEPROM_LRM_EXTERNAL) != LINK_OK)
      return CU_ERROR;
  bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1, SFP_TEMPERATURE, sizeof(SFP_CALIBRATE_DATA),(UINT8 *)&cal_data);
  if(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER)
    if(hal_fluffy_select_ext_lrm_optic(port, FLUFFY_EEPROM_LRM_INTERNAL) != LINK_OK)
      return CU_ERROR;

  if (bytes != sizeof(SFP_CALIBRATE_DATA)) 
    return CU_ERROR; 
  port_optic->temp_reg = cal_data.temp_reg;
  port_optic->voltage = (UINT16)cal_data.voltage;
  port_optic->tx_bias_current_reg = (UINT16)cal_data.TX_bias;
  port_optic->tx_power_reg = (UINT16)cal_data.TX_power;
  port_optic->rx_power_reg = (UINT16)cal_data.RX_power;
  if(debugGlobal.system.mibdom) {
    UINT8 buf_[12]; // test and debug
    register i=0;
    memset(&buf_[0], 0x0, 12);
    memcpy(&buf_[0],(UINT8 *)&cal_data, 10);
    for(i=0;i<12;i+=4) uprintf("buf[%d]: %x %x %x %x ",i,buf_[i],buf_[i+1], buf_[i+2], buf_[i+3]);
    uprintf("\n"); 
  }

  sfp_get_thresholds(port, (void *) (&port_optic->thresholds));

  return CU_OK;
}

static int
stack_member_sfpp_sfp_get_measure(PORT_ID port, pp_stack_port_optic_ipc_header *mem_port_optic)
{
  SFP_CALIBRATE_DATA cal_data;
  int bytes=0, threshold_len=0, index;
  int stackId = PORT_TO_STACK_ID(port);

  if(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER)
    if(hal_fluffy_select_ext_lrm_optic(port, FLUFFY_EEPROM_LRM_EXTERNAL) != LINK_OK)
      return CU_ERROR;
  bytes = sfp_media_read(port, SFP_EEPROM_ADDR_1, SFP_TEMPERATURE, sizeof(SFP_CALIBRATE_DATA),(UINT8 *)&cal_data);
  threshold_len = sfp_media_read(port, SFP_EEPROM_ADDR_1, SFP_MONITOR_THRESHOLD_OFFSET, SFP_MONITOR_THRESHOLD_LEN,(UINT8 *)&mem_port_optic->sfp_thresholds);
  if(SPTR_PORT_DB(port)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER)
    if(hal_fluffy_select_ext_lrm_optic(port, FLUFFY_EEPROM_LRM_INTERNAL) != LINK_OK)
      return CU_ERROR;

  if (bytes != sizeof(SFP_CALIBRATE_DATA) || (threshold_len != SFP_MONITOR_THRESHOLD_LEN)) 
    return CU_ERROR; 
  mem_port_optic->temp_reg = (signed short)cal_data.temp_reg;
  mem_port_optic->voltage = (UINT16)cal_data.voltage;
  mem_port_optic->tx_bias_current_reg = (UINT16)cal_data.TX_bias;
  mem_port_optic->tx_power_reg = (UINT16)cal_data.TX_power;
  mem_port_optic->rx_power_reg = (UINT16)cal_data.RX_power;
  if(debugGlobal.system.mibdom) {
    UINT8 buf_[12]; // test and debug
    register i=0;
    memset(&buf_[0], 0x0, 12);
    memcpy(&buf_[0],(UINT8 *)&cal_data, 10);
    uprintf("stack_member_sfpp_sfp_get_measure() port %p\n", port);
    for(i=0;i<12;i+=4) uprintf("buf[%d]: %x %x %x %x ",i,buf_[i],buf_[i+1], buf_[i+2], buf_[i+3]);
    uprintf("\n"); 
  }

  index = get_port_sfp_monitor_index(port);
  if(index != -1)
    memcpy((void *)&sfp_monitor[stackId][index].thresholds,(void *)&mem_port_optic->sfp_thresholds, SFP_MONITOR_THRESHOLD_LEN);

  return CU_OK;
}

void debug_calibration_print(PORT_ID port, UINT32 temp, UINT16 tx_bias_current, UINT16 rx_power)
{
  UINT32 val_temp=0x0,val_tx_pwr=0x0,val_rx_pwr=0x0;
  int precision = 0x0;

  hal_sfp_monitor_thresholds_display(port);
  val_temp = (int)temp>>8;
  if(temp & 0x8000)
    val_temp |= 0xffffff00;
  precision = (int)(((temp & 0xff) * 10000)/256);
  mw2dbm(tx_bias_current, &val_tx_pwr);
  mw2dbm(rx_power, &val_rx_pwr);
  uprintf("\nport %p Temp %3d.%04d C ", port, val_temp, precision);
  uprintf("TX Power -%03d.%04d dBm ", val_tx_pwr/10000, val_tx_pwr%10000);
  uprintf("RX Power -%03d.%04d dBm ", val_rx_pwr/10000, val_rx_pwr%10000);
  uprintf("TX Bias %3d.%03d mA\n",tx_bias_current/500, (tx_bias_current*2)%1000);
  return 0;
}

static int sfp_get_calibration_measure(PORT_ID lport)
{
	SFP_CALIBRATE_DATA cal_data;
	int stackId = PORT_TO_STACK_ID(lport);
	PORT_ID stack_port = PORT_TO_STACK_PORT(lport);
	int len = sizeof(SFP_CALIBRATE_DATA);
	int index, bytes = 0;
	int temp,precision;
	UINT32 val_temp, val_tx_pwr, val_rx_pwr;

	if(SPTR_PORT_DB(lport)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER)
    if(hal_fluffy_select_ext_lrm_optic(lport, FLUFFY_EEPROM_LRM_EXTERNAL) != LINK_OK)
      return CU_ERROR;
	bytes = sfp_media_read(lport, SFP_EEPROM_ADDR_1, SFP_TEMPERATURE, len,(UINT8 *)&cal_data);
	if(SPTR_PORT_DB(lport)->port_oper_info.media_type == LINK_MEDIA_10GBASE_LRM_ADAPTER)
    if(hal_fluffy_select_ext_lrm_optic(lport, FLUFFY_EEPROM_LRM_INTERNAL) != LINK_OK)
      return CU_ERROR;

	if (bytes != len) 
		return LINK_ERROR; 
	else {
		index = get_port_sfp_monitor_index(lport);
		if (debugGlobal.system.mibdom) {
			hal_sfp_monitor_thresholds_display(lport);
			val_temp = (int)cal_data.temp_reg>>8;
			if(cal_data.temp_reg & 0x8000)
				val_temp |= 0xffffff00;
			precision = (int)(((cal_data.temp_reg & 0xff) * 10000)/256);
			mw2dbm(cal_data.TX_power, &val_tx_pwr);
			mw2dbm(cal_data.RX_power, &val_rx_pwr);
			uprintf("%d sec stackId %d MY_BOOTUP_STACK_ID %d Interval expired to get DOM - SFP %p\n", g_time_counter/10, 
				stackId, MY_BOOTUP_STACK_ID, lport);
			uprintf("\nTemp %3d.%04d C ", val_temp, precision);
			uprintf("Voltage %1d.%04d dBm ", cal_data.voltage/10000, cal_data.voltage%10000);
			uprintf("TX Power -%02d.%04d dBm ", val_tx_pwr/10000, val_tx_pwr%10000);
			uprintf("RX Power -%02d.%04d dBm ", val_rx_pwr/10000, val_rx_pwr%10000);
			uprintf("TX Bias %2d.%03d mA\n",cal_data.TX_bias/500, (cal_data.TX_bias*2)%1000);
		}
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data,(UINT8 *)&cal_data,len);
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.thresholds,
			(UINT8 *) &sfp_monitor[stackId][index].thresholds,sizeof (SFP_MONITOR_THRESHOLDS));
	}

	return CU_OK;
}

static int qsfp_get_calibration_measure(PORT_ID lport)
{
	QSFP_CALIBRATE_DATA qcal_data;
	int stackId = PORT_TO_STACK_ID(lport);
	PORT_ID stack_port = PORT_TO_STACK_PORT(lport);
	int len = sizeof(QSFP_CALIBRATE_DATA);
	int index, bytes;
	int temp,precision;
	UINT32 val_temp, val_voltage, val_rx_pwr;
	UINT32 val_rx2_pwr, val_rx3_pwr,val_rx4_pwr;

 	bytes = sfp_media_read(lport, SFP_EEPROM_ADDR, QSFP_TEMPERATURE, len, (UINT8 *)&qcal_data);

	if (bytes != len) 
		return LINK_ERROR; 
	else {

		index = get_port_sfp_monitor_index(lport);
		if (debugGlobal.system.mibdom) {
			hal_sfp_monitor_thresholds_display(lport);
			val_temp = (int)qcal_data.temp_reg>>8;
			if(qcal_data.temp_reg & 0x8000)
				val_temp |= 0xffffff00;
			precision = (int)(((qcal_data.temp_reg & 0xff) * 10000)/256);
			mw2dbm(qcal_data.RX_power, &val_rx_pwr);
			uprintf("%d sec stackId %d MY_BOOTUP_STACK_ID %d Interval expired to get DOM - QSFP %p\n\n", g_time_counter/10, 
				stackId, MY_BOOTUP_STACK_ID, lport);
			uprintf("\nTemper %3d.%04d C ", val_temp, precision);
			uprintf("Voltage  %1d.%04d Volts", qcal_data.voltage/10000, qcal_data.voltage%10000);
			uprintf("RX Power -%02d.%04d dBm", val_rx_pwr/10000, val_rx_pwr%10000);
			uprintf("TX Bias %2d.%03d mA",(qcal_data.TX_bias)/500, ((qcal_data.TX_bias)*2)%1000);
			
			mw2dbm(qcal_data.RX_power, &val_rx_pwr);
			mw2dbm(qcal_data.RX2_power, &val_rx2_pwr);
			mw2dbm(qcal_data.RX3_power, &val_rx3_pwr);
			mw2dbm(qcal_data.RX4_power, &val_rx4_pwr);
			uprintf("\n\n Chan Rx Power #1  Rx Power #2    Rx Power #3  Rx Power #4\n");
			uprintf("+----+-----------+--------------+--------------+---------------+\n");
			uprintf("  -%02d.%04d dBm", val_rx_pwr/10000, val_rx_pwr%10000);
			uprintf("  -%02d.%04d dBm", val_rx2_pwr/10000, val_rx2_pwr%10000);
			uprintf("  -%02d.%04d dBm", val_rx3_pwr/10000, val_rx3_pwr%10000);
			uprintf("  -%02d.%04d dBm\n", val_rx4_pwr/10000, val_rx4_pwr%10000);
			
			uprintf("\n\n Chan  Tx Bias #1  Tx Bias #2   Tx Bias #3   Tx Bias #4\n");
			uprintf("+----+-----------+--------------+--------------+---------------+\n");
			uprintf("   %2d.%03d mA", qcal_data.TX_bias/500, (qcal_data.TX_bias*2)%1000);
			uprintf("   %2d.%03d mA", qcal_data.TX2_bias/500, (qcal_data.TX2_bias*2)%1000);
			uprintf("   %2d.%03d mA", qcal_data.TX3_bias/500, (qcal_data.TX3_bias*2)%1000);
			uprintf("   %2d.%03d mA\n", qcal_data.TX4_bias/500, (qcal_data.TX4_bias*2)%1000);
		}

		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.temp_reg,(UINT8 *)&qcal_data.temp_reg,2);
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.Vcc,(UINT8 *)&qcal_data.voltage,2);
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.RX_power,(UINT8 *)&qcal_data.RX_power,8);
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.TX_bias,(UINT8 *)&qcal_data.TX_bias,2);
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.TX2_bias,(UINT8 *)&qcal_data.TX2_bias,6);
		memcpy((UINT8 *)&dom_config[MY_BOOTUP_STACK_ID][index].dom_data.thresholds,
			(UINT8 *) &sfp_monitor[stackId][index].thresholds,sizeof (SFP_MONITOR_THRESHOLDS));
	}

	return CU_OK;
}

int sw_cu_get_calibration_measure(PORT_ID l_domport)
{
	if (is_qsfp_optic_dom_supported(l_domport))
			qsfp_get_calibration_measure(l_domport);
	else
		sfp_get_calibration_measure(l_domport);

	return CU_OK;
}

