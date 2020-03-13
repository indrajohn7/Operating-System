#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<signal.h>
//#include<util/util.h>
#include<sys/time.h>
#include<assert.h>

#define NUM_THREAD 2

void *thr_func1(void*);
void *thr_func2(void*);

void* array_func_ptr[NUM_THREAD]={
	thr_func1,
	thr_func2
};

typedef struct __thread_data__{
	int tid;
	double data;
	pthread_mutex_t lock;
	pthread_mutexattr_t attr;
	pthread_cond_t cond;
	void* array_func_ptr[NUM_THREAD];
}thread_data_t;

int counter=0;
	
pthread_t thr[NUM_THREAD];

void sig_handler(int signum)
{
	if(signum!=SIGINT){
		printf("RECIEVED invalid signum=%d in sig_handler()::\n",signum);
		//ASSERT(signum==SIGINT);
	}

	printf("Recieving SIGINT. Exiting Application:\n");

	int i=0;
	for(i=0;i<NUM_THREAD;i++)
		pthread_cancel(thr[i]);
	
	exit(0);
}


void counter_oper(int thread_ID)
{
	struct timeval ts;
	gettimeofday(&ts,NULL);
   	int i;
    for(i=0;i<5;i++){
		counter+=i;
		printf("\n%06lu.%06lu: ---- Thread_ID=%d Run_Cnt=%d   ---\n",(unsigned int)ts.tv_sec,(unsigned int)ts.tv_usec,thread_ID,counter);
		usleep(50);
	}
}

void *thr_func1(void* arg)
{
	thread_data_t* th_data=(thread_data_t*)arg;
	printf("Hello from thread:%d with data:%u",th_data->tid,th_data->data);
	pthread_mutex_lock(&th_data->lock);
	while(1){
		pthread_cond_wait(&th_data->cond,&th_data->lock);
		counter_oper(1);
		sleep(1);
	}
	pthread_mutex_unlock(&th_data->lock);
	pthread_exit(NULL);
}




void *thr_func2(void* arg)
{
	thread_data_t* th_data=(thread_data_t*)arg;
	printf("Hello from thread:%d with data:%u",th_data->tid,th_data->data);
	pthread_mutex_lock(&th_data->lock);
	while(1){
		pthread_cond_signal(&th_data->cond);
		counter_oper(1);
		sleep(1);
	}
	pthread_mutex_unlock(&th_data->lock);
	pthread_exit(NULL);
}


int main()
{

	thread_data_t th_data[NUM_THREAD];
	int i,rc;
	signal(SIGINT,sig_handler);
	//init_thread_data(th_data);
	for(i=0;i<NUM_THREAD;i++){
		if(rc=pthread_create(&thr[i],NULL,th_data[i].array_func_ptr[0],&th_data[i])){
		fprintf(stderr,"error:pthread create value:%d",rc);
		return EXIT_FAILURE;
		}
	}
	


	for(i=0;i<NUM_THREAD;i++)
		pthread_join(thr[i],NULL);

	sig_handler(SIGINT);
	
	return 0;
}

