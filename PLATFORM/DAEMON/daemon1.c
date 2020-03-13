#include <stdio.h>    //printf(3)
#include <stdlib.h>   //exit(3)
#include <unistd.h>   //fork(3), chdir(3), sysconf(3)
#include <signal.h>   //signal(3)
#include <sys/stat.h> //umask(3)
#include <syslog.h>   //syslog(3), openlog(3), closelog(3)
#include<sys/time.h>
#include<assert.h>
#include<mqueue.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>
#include"daemon1.h"
#include<string.h>
#include<math.h>

#define my_mq_name "/my_mq"

char* stream[20]={  "Indra0",
					"Indra1",
					"Indra2",
					"Indra3",
					"Indra4",
					"Indra5",
					"Indra6",
					"Indra7",
					"Indra8",
					"Indra9",
					"Indra10",
					"Indra11",
					"Indra12",
					"Indra13",
					"Indra14",
					"Indra15",
					"Indra16",
					"Indra17",
					"Indra18",
					"Indra19",
					};

void initialize_port_db();
void print_port_db();
void initialize_dummy_port_db();
void populate_dummy_port_db();


static struct mq_attr my_mq_attr; //message queue attribute
static mqd_t my_mq;  //message queue FILE descriptor variable
/*
This is a global file descriptor and when we will create the other threads from the main then we will 
actually use the same global my_mq to do the operations like mq_recieve and mq_send();
*/
pthread_t t1;
pthread_t t2;
pthread_mutex_t lock_x;
int counter=0;

#define NUM_THREAD 5
struct dummy_port_db* g_dummy_port_db[NUM_THREAD];
struct port_db* g_port_db[NUM_THREAD];


void sig_handler(int signum)
{
	if(signum!=SIGINT){
		printf("RECIEVED invalid signum=%d in sig_handler()::\n",signum);
		//ASSERT(signum==SIGINT);
	}
	
	printf("Recieving SIGINT. Exiting Application:\n");
	
	pthread_cancel(t1);
	pthread_cancel(t2);
	
	mq_close(my_mq); //clear the message queue
	mq_unlink(my_mq_name);  //To unlink the message queue from linux file system
	
	exit(0);
}


void counter_oper(int thread_ID)
{
	struct timeval ts;
	pthread_mutex_lock(&lock_x);
	/*This below portion of the code is protected with mutex lock and operating system will make sure that the locked mutex
	 will be able to use the code until unlock happens, */
	gettimeofday(&ts,NULL);
	int i;
	for(i=0;i<5;i++){
		counter+=i;
		printf("\n%06lu.%06lu: ---- Thread_ID=%d Run_Cnt=%d   ---\n",(unsigned int)ts.tv_sec,(unsigned int)ts.tv_usec,thread_ID,counter);
		usleep(50);
	}
	// protectd region ends along with unlock calls
	pthread_mutex_unlock(&lock_x);
}

void initialize_port_db()
{
	int i;
	for(i=0;i<NUM_THREAD;i++){
		g_port_db[i]=(struct port_db*)malloc(sizeof(struct port_db));
		g_port_db[i]->name=(char*)malloc(100);
	}
}


void print_port_db()
{
	int i;
	printf("INDEX	NAME	AGE		SALARY\n");
	for( i=0;i<NUM_THREAD;i++){
		printf("%d	%s	%d	%d\n",i+1,g_port_db[i]->name,g_port_db[i]->age,g_port_db[i]->salary);	
	}
}

void thread1_main()  //Receiving Operation
{
	int status;
	unsigned int exec_secs;
	struct timeval ts;  //learn it
	
	exec_secs=3;
	
	printf("Thread1 Execution started with period:%d uSecs\n",exec_secs);
	
//	pthread_mutex_lock(&lock_x);
	while(1){  //In this condition handler we can implement a conditional handler ---------->
														/*	CODE BLOCK::							pthread_mutex_lock(&count_lock);
																				while (count < MAX_COUNT) {
																				pthread_cond_wait(&count_cond, &count_lock);
																							}*/


		status=mq_receive(my_mq,(char*)g_port_db,sizeof(struct port_db),NULL);
		printf("Thread1 recieve status:%d\n",status);	
		print_port_db();
		sleep(exec_secs);
		//counter_oper(1);
	}
}


void thread2_main()   //Sending Operation
{
	int status;
	unsigned int exec_secs;
	struct timeval ts;  //learn it
	int rcv_cnt;
	
	exec_secs=3;
	
	printf("Thread2 Execution started with period:%d uSecs\n",exec_secs);
	while(1){	
		
		populate_dummy_port_db();
		status=mq_send(my_mq,(char*)g_dummy_port_db,sizeof(struct dummy_port_db),1);
		if(status){
			counter+=1;
		}

		sleep(exec_secs);
	}
}


void initialize_dummy_port_db()
{
	int i;
	for(i=0;i<NUM_THREAD;i++){
		g_dummy_port_db[i]=(struct dummy_port_db*)malloc(sizeof(struct dummy_port_db));
		g_dummy_port_db[i]->name=(char*) malloc(100);	
	}

}

void populate_dummy_port_db()
{
	int i;
	for(i =0;i<NUM_THREAD;i++){
		g_dummy_port_db[i]->name = (char*)malloc(100);
		int idx=rand()%20;
		memcpy(g_dummy_port_db[i]->name,stream[idx],strlen(stream[idx]));
		g_dummy_port_db[i]->age=rand()%25;
		g_dummy_port_db[i]->salary=rand()%((int)pow(10,9));
	}
}


int
daemonize(char* name, char* path, char* outfile, char* errfile, char* infile )
{
    if(!path) { path="~/CODE/PLATFORM/"; }
    if(!name) { name="medaemon"; }
    if(!infile) { infile="/dev/null"; }
    if(!outfile) { outfile="/dev/null"; }
    if(!errfile) { errfile="/dev/null"; }
    //printf("%s %s %s %s\n",name,path,outfile,infile);
    pid_t child;
    //fork, detach from process group leader
    if( (child=fork())<0 ) { //failed fork
        fprintf(stderr,"error: failed fork\n");
        exit(EXIT_FAILURE);
    }
    if (child>0) { //parent
        exit(EXIT_SUCCESS);
    }
    if( setsid()<0 ) { //failed to become session leader
        fprintf(stderr,"error: failed setsid\n");
        exit(EXIT_FAILURE);
    }

    //catch/ignore signals
    signal(SIGCHLD,SIG_IGN);
    signal(SIGHUP,SIG_IGN);

    //fork second time
    if ( (child=fork())<0) { //failed fork
        fprintf(stderr,"error: failed fork\n");
        exit(EXIT_FAILURE);
    }
    if( child>0 ) { //parent
        exit(EXIT_SUCCESS);
    }

	//new file permissions
    umask(0);
    //change to path directory
    chdir(path);

    //Close all open file descriptors
    int fd;
    for( fd=sysconf(_SC_OPEN_MAX); fd>0; --fd )
    {
        close(fd);
    }

    //reopen stdin, stdout, stderr
    stdin=fopen(infile,"r");   //fd=0
    stdout=fopen(outfile,"w+");  //fd=1
    stderr=fopen(errfile,"w+");  //fd=2

    //open syslog
    openlog(name,LOG_PID,LOG_DAEMON);
    return(0);
}

int main()
{
    int res;
    int ttl=120;
    int delay=5;
	pthread_attr_t attr;
	int status;
	signal(SIGINT,sig_handler);
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,1024*1024);
	
	my_mq_attr.mq_maxmsg=100;
	my_mq_attr.mq_msgsize=sizeof(struct port_db);
	
	my_mq=mq_open(my_mq_name,
		O_CREAT | O_RDWR | O_NONBLOCK,0666,&my_mq_attr);
		

	FILE* fp=fopen("syslog.txt","w+");


	initialize_port_db();
	printf("Creating Threads1::\n");   //Get from msg_queue and populate global and print
	status=pthread_create(&t1,&attr,(void*)&thread1_main,NULL);
	if(status!=0){
		printf("Failed to create Thread1 with Status:%d\n",status);
		//ASSERT(status==0);
	}
	
	initialize_dummy_port_db();
	printf("Creating Daemon::\n");
	if( (res=daemonize("mydaemon","~/CODE/PLATFORM",NULL,NULL,NULL)) != 0 ) {
        fprintf(stderr,"error: daemonize failed\n");
        exit(EXIT_FAILURE);
    }


//	while( ttl>0 ) {
        //daemon code here
		
//		initialize_dummy_port_db();
//		populate_dummy_port_db();
		thread2_main();  //has to be a timer callback
        syslog(LOG_NOTICE,"daemon ttl %d",ttl);
		fprintf(fp,"logging info...+delay:%d\n",delay);
		fflush(fp);
        sleep(delay);
        ttl-=delay;
 //   }
   

	
	syslog(LOG_NOTICE,"daemon ttl expired");
	pthread_join(t1,NULL);
	
	sig_handler(SIGINT);
    closelog();
	fclose(fp);
    return(EXIT_SUCCESS);
}

