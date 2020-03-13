//////////////////////////////////////////  compile with: gcc -pthread mutex.c /////////////////////////////////////////


#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
//#include<util/util.h>
#include<sys/time.h>
#include<assert.h>

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
	
	exit(0);
}


void counter_oper(int thread_ID)
{
	struct timeval ts;
	pthread_mutex_lock(&lock_x);
	gettimeofday(&ts,NULL);
	int i;
	for(i=0;i<5;i++){
		counter+=i;
		printf("\n%06lu.%06lu: ---- Thread_ID=%d Run_Cnt=%d   ---\n",(unsigned int)ts.tv_sec,(unsigned int)ts.tv_usec,thread_ID,counter);
		usleep(50);
	}
	pthread_mutex_unlock(&lock_x);
}

void thread1_main()
{
	unsigned int run_cnt;
	unsigned int exec_usecs;
	struct timeval ts;  //learn it
	
	exec_usecs=1000000;
	
	printf("Thread1 Execution started with period:%d uSecs\n",exec_usecs);
	run_cnt=0;
//	pthread_mutex_lock(&lock_x);
	while(1){  //In this condition handler we can implement a conditional handler ---------->
														/*	CODE BLOCK::							pthread_mutex_lock(&count_lock);
																				while (count < MAX_COUNT) {
																				pthread_cond_wait(&count_cond, &count_lock);
																							}*/
		usleep(exec_usecs);
		counter_oper(1);
	}
}


void thread2_main()
{
	unsigned int run_cnt;
	unsigned int exec_usecs;
	struct timeval ts;  //learn it
	
	exec_usecs=1000000;
	
	printf("Thread2 Execution started with period:%d uSecs\n",exec_usecs);
	run_cnt=0;
	while(1){
		usleep(exec_usecs);
		counter_oper(2);
	}
}

int main()
{
	pthread_attr_t attr;
	int status;
	signal(SIGINT,sig_handler);
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr,1024*1024);
	
	
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
