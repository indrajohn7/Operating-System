/*
////////////////////////////////////////////////////////////////MESSAGE QUEUE///////////////////////////////////////////////////////////////
                                     ************COMPILE WITH ::  gcc -pthread ipc.c -lrt **********************
Message Queue is a kind of file system in linux which used as a shared resource in an operating system and Threads or processes can implement various 
functions into it like reading and writing via send and recieve operation.
*/

#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
//#include<util/util.h>
#include<sys/time.h>
#include<assert.h>
#include<mqueue.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<errno.h>

#define my_mq_name "/my_mq"

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

void thread1_main()  //Sending Operation
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
		status=mq_send(my_mq,(const char*)&counter,sizeof(counter),1);
		sleep(exec_secs);
		//counter_oper(1);
	}
}


void thread2_main()   //Recieving Operation
{
	int status;
	unsigned int exec_secs;
	struct timeval ts;  //learn it
	int rcv_cnt;
	
	exec_secs=2;
	
	printf("Thread2 Execution started with period:%d uSecs\n",exec_secs);
	
	while(1){
		status=mq_receive(my_mq,(char*)&rcv_cnt,sizeof(rcv_cnt),NULL);
		if(status){
			printf("RCVD MESSAGE IN THRD2 IS:%d\n",rcv_cnt);
			counter+=1;
		}
		sleep(exec_secs);
	}
}

int main()
{
	pthread_attr_t attr;
	int status;
	signal(SIGINT,sig_handler);
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,1024*1024);
	
	my_mq_attr.mq_maxmsg=10;
	my_mq_attr.mq_msgsize=sizeof(counter);
	
	my_mq=mq_open(my_mq_name,
		O_CREAT | O_RDWR | O_NONBLOCK,0666,&my_mq_attr);
		
	printf("Creating Threads1::\n");
	status=pthread_create(&t1,&attr,(void*)&thread1_main,NULL);
	if(status!=0){
		printf("Failed to create Thread1 with Status:%d\n",status);
		//ASSERT(status==0);
	}
	
	printf("Creating Threads2::\n");
	status=pthread_create(&t2,&attr,(void*)&thread2_main,NULL);
	if(status!=0){
		printf("Failed to create Thread2 with Status:%d\n",status);
		//ASSERT(status==0);
	}
	
	pthread_join(t1,NULL);
	pthread_join(t2,NULL);
	
	sig_handler(SIGINT);
	
	return 0; 
}
