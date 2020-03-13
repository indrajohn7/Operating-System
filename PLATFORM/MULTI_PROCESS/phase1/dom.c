#include"dom.h"


typedef unsigned short PORT_ID;
#if 0
#define MAX_LOCAL_PORTS 0xff
#define MAX_PORT_PER_SLOT 64
#define PORT_TO_MODULE_ID_LOCAL(port_id)    ((((port_id) & 0x00c0) >> 6))
#define MAKE_MODULE_ID(stack, local_module)     ((((stack)-1)*MAX_LOCAL_SLOT + (local_module))) // module: 0 - (max_stacking_num * 4). // CHEETAH_STACKING
#define MAKE_PORTID(module, module_port)    ((PORT_ID) (((module) << 6) | (module_port))) // module: 0 - (max_stacking_num * 4).
#define PORT_TO_MODULE_PORT(port_id)        (((port_id) & 0x003F))  // return 0-63
#endif
#define SECOND 60

#define QSFP_OPTIC 1
#define SFP_OPTIC 2

#define QSFP_MON_INT_FLAGS         3
#define QSFP_MON_INT_LEN           10
#define QSFP_TEMPERATURE           22
#define QSFP_TEMPERATURE_LEN       2

#define QSFP_THRESHOLD_OFFSET      128
#define QSFP_THRESHOLD_LEN         64

#define SFP_MONITOR_THRESHOLD_OFFSET    0
#define SFP_MONITOR_THRESHOLD_LEN      40
#define SFP_MONITOR_FLAG_OFFSET      112
#define SFP_MONITOR_FLAG_LEN           8
#define SFP_TEMPERATURE           96
#define SFP_TEMPERATURE_LEN        2

#define SFP_EEPROM_ADDR_1          0x51
#define SFP_EEPROM_ADDR	           0x50
#define SFP_EEPROM_ADDR_STD        0x50

/*
struct OM_PORTS{
	int stack_id;
	int local_ports;
	int interval;
	int optic;
};
*/
struct OM_PORTS om_ports;
struct OM_PORTS om_port_list[MAX_LOCAL_PORTS];
/*
struct om_port_data{
	PORT_ID port;
	char thresold_data[64];
	char optic_data[64];
	int threshold_error;
	int optic_error;
};
*/
/*Better to maintain the array list due to multiple thread collission*/
struct om_port_data om_data[MAX_LOCAL_PORTS];

int om_enabled_port_list[MAX_LOCAL_PORTS];

void populate_om_port_list_from_FI(struct OM_PORTS dom_port)
{
	printf("\r populate_om_port_list_from_FI \r\n");
	int port = dom_port.local_ports;
//	om_enabled_port_list[port] = dom_port.interval * SECOND;
	memcpy(&om_port_list[port], &dom_port, sizeof(struct OM_PORTS));
	
}


void worker_thread_dom(int port)
{
	printf("\r worker_thread_dom \r\n");
	int stack;
	if (port == -1){
		port = om_ports.local_ports;
		om_enabled_port_list[om_ports.local_ports] = om_ports.interval * SECOND;
		memcpy(&om_port_list[port], &om_ports, sizeof(struct OM_PORTS));
		/*
		om_port_list[port].local_port = om_ports.local_port;
		om_port_list[port].interval = om_ports.interval;
		om_port_list[port].optic = om_ports.optic;
		*/
		stack = om_ports.stack_id;
	}
	
	if (om_enabled_port_list[port] == 0)
		return;
	
	else{
		if ((om_enabled_port_list[port] - TIMER_INTERVAL) <= 0){
			om_enabled_port_list[port] = om_port_list[port].interval * SECOND;
			get_sfp_eeprom_data(port);
		}else{
			om_enabled_port_list[port] -= TIMER_INTERVAL;
		}
	}

	
	//call SIL level APIs and populate om_data and send back to FI
}


void get_sfp_eeprom_data(int port)
{	
	printf("\r get_sfp_eeprom_data \r\n");
	char buf[64];
	int stack_id = om_ports.stack_id;
	int local_module = PORT_TO_MODULE_ID_LOCAL(port); 
	int module = MAKE_MODULE_ID(stack_id,local_module);
	int module_port = PORT_TO_MODULE_PORT(port);

	PORT_ID port_id = MAKE_PORTID(module,module_port);
	
	int ret,page;
	ret = sys_port_present(local_module, module_port);
	page = 0;
	memset(&om_data[port],0x00,sizeof(struct om_port_data));
	
	om_data[port].port = port_id;
	if(ret == 1){
		if(om_port_list[port].optic == QSFP_OPTIC){
			//threshold data collectioon
			
			get_threshold_data(module_port, local_module, QSFP_OPTIC, om_data[port].threshold_data, &om_data[port].threshold_error);
			//optic power data collection
			get_optic_data(module_port, local_module, QSFP_OPTIC, om_data[port].optic_data, &om_data[port].optic_error);
		}else if (om_port_list[port].optic == SFP_OPTIC){
		
			//threshold data collection
			get_threshold_data(module_port, local_module, SFP_OPTIC, om_data[port].threshold_data, &om_data[port].threshold_error);
			//optic power data collection
			get_optic_data(module_port, local_module, SFP_OPTIC, om_data[port].optic_data, &om_data[port].optic_error);
		/*		
			if (sfp_media_read(module_port, local_module, SFP_EEPROM_ADDR_1, SFP_MONITOR_FLAG_OFFSET,
					SFP_MONITOR_FLAG_LEN, buf) != SFP_MONITOR_FLAG_LEN){
			}else{
				
				printf("Media read is failed in the port: %d\n",port);
			}
		*/
		}

		if(om_data[port].threshold_error == 1 || om_data[port].optic_error == 1)
			om_enabled_port_list[port] = 0;

	}else{
		printf("Optic absent in the port: %d\n",port);
	}
}


void get_threshold_data(int module_port, int local_module, int optic_type, char* buff, int* err)
{
	printf("\r get_threshold_data \r\n");
	int ret,page=0;

	*err = 0;
	if(optic_type == QSFP_OPTIC){
		
		memset(buff, 0x00, QSFP_THRESHOLD_LEN);
		
		ret = sys_qsfp_page_read(local_module, module_port, QSFP_MON_INT_FLAGS, QSFP_THRESHOLD_OFFSET,
					buff, QSFP_THRESHOLD_LEN);
		if(ret != QSFP_THRESHOLD_LEN){
			printf("QSFP Threshold read failed for module: %d port:%d \n",local_module,module_port);
			*err = 1;
		}
	}else if(optic_type == SFP_OPTIC){
		
		memset(buff, 0x00, SFP_MONITOR_THRESHOLD_LEN);
		
		page = SFP_EEPROM_ADDR_1 - SFP_EEPROM_ADDR_STD;
		ret = sys_qsfp_page_read(local_module, module_port, page, SFP_MONITOR_THRESHOLD_OFFSET,
					buff, SFP_MONITOR_THRESHOLD_LEN);
		if(ret != SFP_MONITOR_THRESHOLD_LEN){
			/*FI sends out a syslog in this stage for latching error in threshold*/
			int local_port = (local_module * MAX_PORT_PER_SLOT) + module_port;
			om_enabled_port_list[local_port] = 0;
			*err = 1;
			printf("SFP Threshold read failed for module: %d port:%d \n",local_module,module_port);
		}
	}
}




void get_optic_data(int module_port, int local_module, int optic_type, char* buff, int* err)
{
	printf("\r get_optic_data \r\n");
	int ret,page=0;
	
	*err = 0;
	if(optic_type == QSFP_OPTIC){

		memset(buff, 0x00, QSFP_MON_INT_LEN);
		
		page = SFP_EEPROM_ADDR - SFP_EEPROM_ADDR_STD;
		ret = sys_qsfp_page_read(local_module, module_port, page, QSFP_MON_INT_FLAGS,
					buff, QSFP_MON_INT_LEN);
		if(ret != QSFP_MON_INT_LEN){
			printf("QSFP Optic read failed for module: %d port:%d \n",local_module,module_port);
			*err = 1;
		}

/* 		if(SPTR_PORT_DB(port)->port_mtype == X40G_STACK_PORT)
      	{
          	buf[0] = (msg[3]&0xc0) | ((msg[4] & 0xc0)>>2); // temp, volt alarm
         	buf[1] = (msg[8]&0xc0)& 0xff; // Bias alarm chan 1
	      	buf[2] = (msg[6]&0xc0) & 0xc0; // RX power alarm chan 1
         	buf[4] = ((msg[3]& 0x30)<<2) | (msg[4] & 0x30); // temp, volt warning
          	buf[5] = ((msg[8]&0x30)<<2)& 0xc0; // Bias warning chan 1
          	buf[6] = ((msg[6]&0x30)<<2) & 0xc0; // RX power warning chan 1
      	}
 *
 * */

	}else if(optic_type == SFP_OPTIC){
	
		/*Talk to Darshan about the failure*/
		memset(buff, 0x00, SFP_MONITOR_FLAG_LEN);
		
		page = SFP_EEPROM_ADDR_1 - SFP_EEPROM_ADDR_STD;
		ret = sys_qsfp_page_read(local_module, module_port, page, SFP_MONITOR_FLAG_OFFSET,
					buff, SFP_MONITOR_FLAG_LEN);
		if(ret != SFP_MONITOR_FLAG_LEN)
			printf("SFP Optic power read failed for module: %d port:%d \n",local_module,module_port);
		else{
			int i;
			for(i = 0; ;){
				if(sys_qsfp_page_read(local_module, module_port, page, SFP_MONITOR_FLAG_OFFSET,
						buff, SFP_MONITOR_FLAG_LEN) == SFP_MONITOR_FLAG_LEN){

					break;
				}else if(i == 5){
					/*Here it sends out latching error message and keep on trying*/
					/*Indra: rework here..
					 *ksprintf(cu_line_buf, "OPTICAL MONITORING: port %p, failed to read latched flags\n", lport);
					 *send_port_optical_msg_type(cu_line_buf, OPTICAL_MONITORING_MSG_TYPE);
					 *status = LINK_ERROR;
					 *
					 * */

					/*I will revert the timer to 0 here to send out the message to FI*/
					int local_port = (local_module * MAX_PORT_PER_SLOT) + module_port;
					om_enabled_port_list[local_port] = 0;
					*err = 1;
					break;
				}
				i++;
			}
		}
	}
}
