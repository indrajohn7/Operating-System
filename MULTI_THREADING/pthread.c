/*  Compile Option:		<gcc -pthread pthread.c>     */
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>

#define Num_Thread 5

typedef struct _thr_data{
	int tid;
	double stuff;

}thread_data_t;


void *func_ptr(void* arg)
{
	thread_data_t *data=(thread_data_t*)arg;
	printf("Hello from thread:%d with data:%u\n",data->tid,data->stuff);
	pthread_exit(NULL);
}

void init_thread_data(thread_data_t* t)
{
	int i=0;
	for(i=0;i<Num_Thread;i++){
		t[i].tid=i+1;
		t[i].stuff=rand()%1000;
	}
}


int main()
{
	pthread_t thr[Num_Thread];
	thread_data_t th_data[Num_Thread];
	int i,rc;
	init_thread_data(th_data);
	for(i=0;i<Num_Thread;i++){
		if(rc=pthread_create(&thr[i],NULL,func_ptr,&th_data[i])){
			fprintf(stderr,"error:pthread create value:%d",rc);
			return EXIT_FAILURE;
		}
	}

	for(i=0;i<Num_Thread;i++)
		pthread_join(thr[i],NULL);

	return 0;
}
