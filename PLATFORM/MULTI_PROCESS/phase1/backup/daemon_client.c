/************* TCP CLIENT CODE *******************/

#include<stdio.h>
#include"dom.h"
#include<pthread.h>
#include <dlfcn.h>
#include<sys/types.h>		//socket
#include<sys/socket.h>		//socket
#include<string.h>		//memset
#include<stdlib.h>		//sizeof
#include<netinet/in.h>		//INADDR_ANY
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include<sys/time.h>
#include<fcntl.h>
#include<errno.h>
#include<string.h>
#include<math.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include<mqueue.h>
#include<arpa/inet.h>

#define my_mq_name "/sbin/my_mq"

//#define TIMER_INTERVAL 10
#define PORT 8000
#define SERVER_IP "127.0.0.1"
#define MAXSZ 100
#define BufferLength 100

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int interrupted;

/*
typedef void (*sys_qsfp_page_read_sil)(int,int,int,int,char*,int);
typedef void (*sys_port_present_sil)(int,int);
*/

void call_server_thread();
void call_client_thread();
void connect_socket(int *);
int message_queue_signal_handler();

int semaphore = 0;

static struct mq_attr my_mq_attr;	//message queue attribute
static mqd_t my_mq;		//message queue FILE descriptor variable

pthread_t t1;
pthread_t t2;

int
daemonize(char *name, char *path, char *outfile, char *errfile, char *infile)
{
	if (!path) {
		path = "/sbin/";
	}
	if (!name) {
		name = "medaemon";
	}
	if (!infile) {
		infile = "/dev/null";
	}
	if (!outfile) {
		outfile = "/dev/null";
	}
	if (!errfile) {
		errfile = "/dev/null";
	}
	//printf("%s %s %s %s\n",name,path,outfile,infile);
	pid_t child;
	//fork, detach from process group leader
	if ((child = fork()) < 0) {	//failed fork
		fprintf(stderr, "error: failed fork\n");
		exit(EXIT_FAILURE);
	}
	if (child > 0) {	//parent
		exit(EXIT_SUCCESS);
	}
	if (setsid() < 0) {	//failed to become session leader
		fprintf(stderr, "error: failed setsid\n");
		exit(EXIT_FAILURE);
	}
	//catch/ignore signals
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	//fork second time
	if ((child = fork()) < 0) {	//failed fork
		fprintf(stderr, "error: failed fork\n");
		exit(EXIT_FAILURE);
	}
	if (child > 0) {	//parent
		exit(EXIT_SUCCESS);
	}
	//new file permissions
	umask(0);
	//change to path directory
	chdir(path);

	//Close all open file descriptors
/*  int fd_open;
    for( fd_open=sysconf(_SC_OPEN_MAX); fd_open>0; --fd_open )
    {
        close(fd_open);
    }
*/
	//reopen stdin, stdout, stderr
	stdin = fopen(infile, "r");	//fd=0
	stdout = fopen(outfile, "w+");	//fd=1
	stderr = fopen(errfile, "w+");	//fd=2

	//open syslog
	openlog(name, LOG_PID, LOG_DAEMON);
	return (0);
}

int fd;
void *lib;

void sig_handler(int signum)
{
	if (signum != SIGINT) {
		printf("RECIEVED invalid signum=%d in sig_handler()::\n",
		       signum);
	}

	printf("Recieving SIGINT. Exiting Application:\n");

	pthread_cancel(t1);
	pthread_cancel(t2);

	mq_close(my_mq);	//clear the message queue
	mq_unlink(my_mq_name);	//To unlink the message queue from linux file system

//      dlclose(lib);

	exit(0);
}

#if 0
void link_sil_lib()
{
	lib = dlopen("", RTLD_LAZY);
	/*link SIL lib */
	sys_qsfp_page_read_sil sys_qsfp_page_read =
	    (sys_qsfp_page_read_sil) dlsym(lib, "sys_qsfp_page_read");

	sys_port_present_sil sys_port_present =
	    (sys_port_present_sil) dlsym(lib, "sys_port_present");
}
#endif

int main()
{
	signal(SIGINT, sig_handler);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);

	int status;
	/*Message Queue */
	my_mq_attr.mq_maxmsg = 1000;
	my_mq_attr.mq_msgsize = 1024;

	my_mq = mq_open(my_mq_name,
			O_CREAT | O_RDWR | O_NONBLOCK, 0666, &my_mq_attr);

	 /*END*/
//      link_sil_lib();
	    /*Initialising om port list timer */
	int port = 0;
	for (port = 0; port <= MAX_LOCAL_PORTS; port++)
		om_enabled_port_list[port] = 0;
	 /*END*/ connect_socket(&fd);

	printf("Creating Threads 2 in Daemon Process::\n");
	status = pthread_create(&t2, &attr, (void *)&call_client_thread, NULL);
	if (status != 0) {
		printf("Failed to create Thread2 with Status:%d\n", status);
	}

	printf("Creating Thread 1 in Daemon Process::\n");
	status = pthread_create(&t1, &attr, (void *)&call_server_thread, NULL);
	if (status != 0) {
		printf("Failed to create Thread1 with Status:%d\n", status);
	}

	/*This is supposed to be inside a thread/continuous call so that transaction continues */
/*	while(1) {
		message_queue_signal_handler(); 
		//NO OPS
	}
 	
*/
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);

	sig_handler(SIGINT);

	return 0;

}

void connect_socket(int *fp1)
{
	int clientSocket, sd2, portNum, nBytes;
	char buffer[1024];
	struct sockaddr_in serverAddr;
	socklen_t addr_size;
	/*Create UDP socket */
	clientSocket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	/*Configure settings in address struct */
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
	/*Initialize size variable to be used later on */
	addr_size = sizeof(serverAddr);

	int n, res, ret;
	char msg1[MAXSZ];
	char msg2[MAXSZ];

	if ((res = daemonize("mydaemon", "/sbin/", NULL, NULL, NULL)) != 0) {
		fprintf(stderr, "error: daemonize failed\n");
		exit(EXIT_FAILURE);
	}

	*fp1 = clientSocket;

connection:
	ret =
	    connect(clientSocket, (struct sockaddr *)&serverAddr,
		    sizeof(serverAddr));
	if (ret != 0)
		goto connection;
}

int message_queue_signal_handler()
{
	/*      Message Queue :: Indra work here
	 *      We should send a signal to wake up the thread once recieves any message from the msg_q
	 *      and collect the necessary data structures sent from FI.
	 * */
//      printf("message_queue_signal_handler \r\n");
	if (mq_receive(my_mq, (char *)&om_ports, sizeof(struct OM_PORTS), NULL)
	    == sizeof(struct OM_PORTS)) {
		printf("message_queue_signal_handler  message recieved\r\n");
		pthread_mutex_lock(&mutex);
		interrupted = 1;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
		return 1;
	}

	return 0;
}

/*	This Thread will be on sleeping state for OM time interval. 
 *	Other than the time interval this thread should also be waken up once it gets the 
 *	msg_q get operation from the dom server thread.
 *
 * */

void call_client_thread()
{
	printf("call_client_thread in daemon\r\n");
	int nBytes, iter;
	char buffer[1024];
	int clientSocket = fd;
	struct timespec ts;
	struct timeval tv;

callback:
	gettimeofday(&tv, NULL);

	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	ts.tv_sec += TIMER_INTERVAL;

	pthread_mutex_lock(&mutex);

	while (!interrupted) {
		int rc = pthread_cond_timedwait(&cond, &mutex, &ts);
		if (rc == ETIMEDOUT) {
			printf("\rTimed out!\n\r");
			//Do the work here
			for (iter = 0; iter < MAX_LOCAL_PORTS; iter++) {
				if (om_enabled_port_list[iter] <= 0)
					continue;
				worker_thread_dom(iter);
				/*      send OM data
				 *      Even if time interval list is 0 then also we send : chances are very less cause
				 *      this gets sorted in FI itself.
				 *      */
				send(clientSocket, &(om_data[iter]),
				     sizeof(struct om_port_data), 0);

				nBytes =
				    recv(clientSocket, &(om_ports),
					 sizeof(struct OM_PORTS), 0);

				populate_om_port_list_from_FI(om_ports);
				if (nBytes == sizeof(struct OM_PORTS))
					printf("Recieved updated port data\n");
			}
			pthread_mutex_unlock(&mutex);
			goto callback;
		}		/*else if (message_queue_signal_handler() == 1){
				   //Do operation. Got MSG_Q      
				   } */
	}
//      while (message_queue_signal_handler() == 1){  //can call the message queue get here
	printf("Woke up!\n");
	interrupted = 0;

	//do work here
	worker_thread_dom(-1);
	//send OM data
	send(clientSocket, &(om_data[om_ports.local_ports]),
	     sizeof(struct om_port_data), 0);

	nBytes = recv(clientSocket, &(om_ports), sizeof(struct OM_PORTS), 0);

	populate_om_port_list_from_FI(om_ports);
	if (nBytes == sizeof(struct OM_PORTS))
		printf("Recieved updated port data\n");

	pthread_mutex_unlock(&mutex);
	goto callback;
//      }

}

/*	The recive thread in DOM process would be in waiting state to recieve any operation from the FI App
 *	Once it recieves the operation and respective data structures then it sends the msg_q to the client_thread 
 *	of DOM process to wake up and do the repective operations.
 *
 *	
 * */

void call_server_thread()
{
	printf("call_server_thread in daemon\r\n");
	int sd2 = fd;
	int rc, temp, length;
	char buffer[1024];
	fd_set read_fd;
	while (1) {

//              FD_ZERO(&read_fd);
//              FD_SET(sd2, &read_fd);
//              rc = select(sd2+1, &read_fd, NULL, NULL, NULL /*&timeout*/);

		/* Read data from the client. */
		int totalcnt = 0;
//                      if((rc == 1) && (FD_ISSET(sd2, &read_fd))){
//                      if(semaphore){

		/*Read the updated global DS from FI */
		rc = read(sd2, &om_ports, sizeof(struct OM_PORTS));
		if (rc < 0) {
			perror("Serverread() error in daemon");
			//              close(sd2);
			//              exit (1);                       
		} else if (rc == 0) {
			printf("Client program has issued a close()\n");
			//              close(sd2);
			//              exit(1); //dont exit():: break from here        
		} else {
			printf
			    ("Data recieved from Main Client to Daemon Server:%s\n",
			     buffer);
			totalcnt += rc;
			printf("Daemon Serverread() is OK\n");
		}
//                      }
//                      }

		/* Shows the data */
		//printf("Received data from the Main client: %s\n", buffer);   

		/* Echo some bytes of string, back */
		/* to the client by using the write() */
		/* function. */
		/*************************************/
		/* write() some bytes of string, */
		/* back to the client. */
		if (rc > 0) {
			//printf("********************Server Echoing back to Main client*********************\n");

			/*Writes down ACK :: As of now this write is a dummy */
			//rc = write(sd2, buffer, totalcnt);
			/*Dont send back any prompt data. send msg q and recieve it in the client thread and operate on the thread and send data to FI */
			/*Message Queue send from Daemon server to Daemon client 
			 *It will recieve operatable datas from FI.  TODO:: Indra
			 * */
			int status =
			    mq_send(my_mq, (char *)&om_ports,
				    sizeof(struct OM_PORTS), 1);
			if (status) {
				printf
				    ("Daemon Server sends the recieved message to Daemon client message queue \n");
			}
		/*END*/}
		/*      This thread wakes up in every 10 seconds to collect any info collected.
		 * */
		sleep(10);

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
	exit(0);
}
