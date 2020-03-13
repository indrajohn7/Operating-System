/**********************    tcpserver.c   *************/
/* header files needed to use the sockets API */
/* File contain Macro, Data Type and Structure */
/***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include<ctype.h>
//#include"optical_monitor_TI.h"
#include"dom_utils.h"
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include<netinet/in.h>		//INADDR_ANY
//#include <syslog.h>
#include <sys/stat.h>
#include<sys/time.h>
#include<fcntl.h>
#include<errno.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include<mqueue.h>
#include<pthread.h>

#define MAXSZ 100
#define BufferLength 100
#define SERVER_IP "127.0.0.1"

/*Server Port*/

#define SERVPORT 8000
#define CLIENTPORT 8001

void call_server_thread();
void call_client_thread();
void connect_socket(int *);
void create_socket(int *, int *);
void send_notification_to_dom(struct OM_PORTS *);

pthread_t t1;
pthread_t t2;

int semaphore = 0;

int fd, fd1, fd2;

int socket_init()
{

	create_socket(&fd1, &fd2);

	return 0;

}

int recieve_thread_init()
{

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);

	int status;
	printf("Creating Threads1::\n");
	status = pthread_create(&t1, &attr, (void *)&call_server_thread, NULL);
	if (status != 0) {
		printf("Failed to create Thread1 with Status:%d\n", status);
		return status;
	}
#if 0
	printf("Creating Threads2::\n");
	status = pthread_create(&t2, &attr, (void *)&call_client_thread, NULL);
	if (status != 0) {
		printf("Failed to create Thread2 with Status:%d\n", status);
		return status;
	}
#endif
	pthread_join(t1, NULL);
//      pthread_join(t2,NULL);

	return 0;

}

/*	Client thread on the FI APP would be called from the CLI layer and this thread will not be in waiting 
 *	state. So, take care accordingly. Send all the required information required through this operation
 *	to the daemon.
 *
 *	Required Data Structures:
 *
 * */

void send_notification_to_dom(struct OM_PORTS *dom_port)
{
	int clientSocket;
	int nBytes, res;

	clientSocket = fd2;
	nBytes = sizeof(struct OM_PORTS);

	send(clientSocket, dom_port, nBytes, 0);
	/*Decide if anything to recieve back after send operation */
	//nBytes = recv(clientSocket,buffer,MAXSZ,0);

}

void create_socket(int *fp1, int *fp2)
{

	/*Variable Structure Definition */
	int sd, sd2, rc, length;
	length = sizeof(int);
	int totalcnt = 0, on = 1;
	char temp;
	char buffer[BufferLength];
	struct sockaddr_in serveraddr;
	struct sockaddr_in their_addr;
	fd_set read_fd;

	/* The socket() function returns a socket descriptor */
	/* representing an endpoint. The statement also */
	/* identifies that the INET (Internet Protocol) */
	/* address family with the TCP transport (SOCK_STREAM) */
	/* will be used for this socket. */
	/************************************************/
	/* Get a socket descriptor */

	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Serversocket()error");
		/* exit */
		//exit(1);
	} else
		printf("Serversocket() is OK \n");

	/* The setsockopt() function is used to allow */
	/* the local address to be reused when the server */
	/* is restarted before the required wait time */
	/* expires */
	/***********************************************/
	/* Allow socket descriptor to be reusable */

	if ((rc =
	     setsockopt(sd, SOL_SOCKET, SO_REUSEADDR /*| TCP_NODELAY */ ,
			(char *)&on, sizeof(on))) < 0) {
		perror("Serversetsockopt()error");
		// close(sd);
		//exit(1);
	} else
		printf("Serversetsockopt()is OK\n");

	/* bind to an address */
	memset(&serveraddr, 0x00, sizeof(struct sockaddr_in));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(SERVPORT);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	//printf("Using %s, listening at %d\n",inet_ntoa(serveraddr.sin_addr), SERVPORT);

	/*After the socket descriptor is created, a bind() */
	/* function gets a unique name for the socket. */
	/* In this case , the user sets the */
	/* s_addr to 0, which allows the system to */
	/* connect to any client that used port 8000. */

	if ((rc =
	     bind(sd, (struct sockaddr *)&serveraddr,
		  sizeof(serveraddr))) < 0) {
		perror("Serverbind() error");
		/* close the socket descriptor */
		// close(sd);
		//exit(1);
	} else
		printf("Serverbind()is OK\n");

	/* The listen() function allows the server to accept */
	/* incoming client connections. In this example, */
	/* the backlog is set to 10. This means that the */
	/* system can queue up to 10 connection requests before */
	/* the system starts rejecting incoming requests. */
	/*************************************************/
	/* Up to 10 clients can be queued */

	if ((rc = listen(sd, 10)) < 0) {
		perror("Serverlisten() error");
		//      close(sd);
		//      exit(1);        
	} else
		printf("ServerReadyfor client connection...\n");

	/* The server will accept a connection request */
	/* The server will accept a connection request */
	/* connection request does the following: */
	/* Is part of the same address family */
	/* Uses streams sockets (TCP) */
	/* Attempts to connect to the specified port */
	/***********************************************/
	/* accept() the incoming connection request. */

	int sin_size = sizeof(struct sockaddr_in);
	if ((sd2 =
	     accept4(sd, (struct sockaddr *)&their_addr, &sin_size,
		     SOCK_NONBLOCK)) < 0) {
		perror("Serveraccept() error");
		//close(sd);
		//exit (1);
	} else
		printf("Serveraccept()is OK\n");

	/*client IP */
	printf("Server new socket, sd2 is OK...\n");
	//printf("Got connection from the client: %s\n",inet_ntoa(their_addr.sin_addr));

	*fp1 = sd;
	*fp2 = sd2;
}

/*	This read thread would be in a  waiting state. and will perform below operation:
 *	1> will recieve sfp monitoring/thresold data from DOM
 *	2> Will send updated global data structures to DOM for next cycle.
 * */

void call_server_thread()
{
	int sd = fd1;
	int sd2 = fd2;

	fd_set read_fd;
	int rc, totalcnt, temp, length;
	char buffer[MAXSZ];
	/* The select() function allows the process to */
	/* the process when the event occurs. In this */
	/* example, the system notifies the process */
	/* only when data is available to read. */
	/***********************************************/
	/* Wait for up to 15 seconds on */
	/* select() for data to be read. */
	while (1) {
		struct timeval timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		FD_ZERO(&read_fd);
		FD_SET(sd2, &read_fd);
		rc = select(sd2 + 1, &read_fd, NULL, NULL, NULL /*&timeout */ );
		if ((rc == 1) && (FD_ISSET(sd2, &read_fd))) {
			/* Read data from the client. */
			totalcnt = 0;
			/*Read the SFP monitoring data */
			rc = read(sd2, &om_data, sizeof(struct om_port_data));
			//logging on recieved data      
			extern void logging_dom_info(struct om_port_data);
			logging_dom_info(om_data);

			if (rc < 0) {
				perror("Serverread() error");
				//close(sd);
				//close(sd2);
				//exit (1);                     
			} else if (rc == 0) {
				printf("Client program has issued a close()\n");
				//close(sd);
				//close(sd2);
				//exit(1); //dont exit():: break from here      
			} else {
				semaphore = 1;
				printf("Data recieved from Client:%s\n",
				       buffer);
				totalcnt += rc;
				printf("Serverread() is OK\n");
			}
		} else {
			//sleep(10);
		}

		/* Echo some bytes of string, back */
		/* to the client by using the write() */
		/* function. */
		/*************************************/
		/* write() some bytes of string, */
		/* back to the client. */

		if (rc > 0) {
			printf
			    ("*******************Server Echoing back to client********************\n");

			int local_port = PORT_TO_LOCAL_PORT(om_data.port);
			unsigned short port = om_data.port;
			if (tanto_optical_monitoring(om_data.port) == 1
			    && tanto_sfp_optical_monitoring_service(om_data.
								    port) ==
			    1) {

				transaction_between_dom_and_FI(1);
#if 0
				om_enabled_port_list[local_port] =
				    (SPTR_PORT_DB(port))->port_config.
				    optical_monitor_interval;
				om_ports.local_port =
				    PORT_TO_LOCAL_PORT(om_data.port);
				om_ports.interval =
				    (SPTR_PORT_DB(port))->port_config.
				    optical_monitor_interval;
				om_ports.stack_id = MY_BOOTUP_STACK_ID;
				if (is_qsfp_optic_dom_supported(port))
					om_ports.optic = 1;
				else
					om_ports.optic = 2;
#endif
			} else {
				transaction_between_dom_and_FI(0);
#if 0
				om_ports.local_port =
				    PORT_TO_LOCAL_PORT(om_data.port);
				om_ports.interval = 0;
				om_ports.stack_id = MY_BOOTUP_STACK_ID;
				om_enabled_port_list[local_port] = 0;
				if (is_qsfp_optic_dom_supported(port))
					om_ports.optic = 1;
				else
					om_ports.optic = 2;
#endif
			}

			/*write operation into dom client */

			rc = write(sd2, &om_ports, sizeof(struct OM_PORTS));
			if (rc != totalcnt) {
				perror("Server write() error");
				/* Get the error number. */
				rc = getsockopt(sd2, SOL_SOCKET, SO_ERROR,
						&temp, &length);
				if (rc == 0) {
					/* Print out the asynchronously */
					/* received error. */
					errno = temp;
					perror("SO_ERROR was: ");
				}
			} else {

			}

			int time_interval =
			    om_enabled_port_list[PORT_TO_LOCAL_PORT
						 (om_data.port)];
		}
		//      sleep(time_interval);
		/*      Have to check whether sleep requires or not?
		 *      As of now not required cause select will be waiting for next connection
		 * */
	}
	/* When the data has been sent, close() */
	/* the socket descriptor that was returned */
	/* from the accept() verb and close() the */
	/* original socket descriptor. */
		/*****************************************/
	/* Close the connection to the client and */
	/* close the server listening socket. */
		/******************************************/

	close(sd2);
	close(sd);
	exit(0);
}
