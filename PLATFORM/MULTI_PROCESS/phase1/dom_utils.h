#ifndef DOM

#define DOM 1
extern int om_enabled_port_list[];

struct OM_PORTS{
	int stack_id;
	int local_port;
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
extern struct om_port_data om_data;

#define PORT_TO_LOCAL_PORT(port) (((port) & 0x00FF))

extern int tanto_optical_monitoring(unsigned short);
extern int tanto_sfp_optical_monitoring_service(unsigned short);
extern void transaction_between_dom_and_FI(int);

#endif DOM
