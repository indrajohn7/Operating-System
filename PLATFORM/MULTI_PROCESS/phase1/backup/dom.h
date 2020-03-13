
#include<stdio.h>
#include<stdlib.h>
#include <dlfcn.h>
#include<string.h>


#ifndef DOM

#define DOM 1

#define TIMER_INTERVAL 10
#define MAX_PORT_PER_SLOT 64
#define MAX_LOCAL_PORTS 0xff
#define MAX_LOCAL_SLOT 4
#define PORT_TO_MODULE_ID_LOCAL(port_id)    ((((port_id) & 0x00c0) >> 6))
#define MAKE_MODULE_ID(stack, local_module)     ((((stack)-1)*MAX_LOCAL_SLOT + (local_module))) // module: 0 - (max_stacking_num * 4).
#define MAKE_PORTID(module, module_port)    ((PORT_ID) (((module) << 6) | (module_port))) // module: 0 - (max_stacking_num * 4).
#define PORT_TO_MODULE_PORT(port_id)        (((port_id) & 0x003F))  // return 0-63

struct OM_PORTS{
	int stack_id;
	int local_ports;
	int interval;
	int optic;
};

struct om_port_data{
	unsigned short port;
	char threshold_data[64];
	char optic_data[64];
	int threshold_error;
	int optic_error;
};


extern struct OM_PORTS om_ports;
extern struct OM_PORTS om_port_list[];

extern struct om_port_data om_data[];

extern int om_enabled_port_list[];



#endif

void get_optic_data(int module_port, int local_module, int optic_type, char* buff, int* err);
void get_threshold_data(int module_port, int local_module, int optic_type, char* buff, int* err);
void get_sfp_eeprom_data(int port);

void worker_thread_dom(int port);
void worker_thread_dom(int port);


