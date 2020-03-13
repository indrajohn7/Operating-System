#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>


#define NUM 5

typedef struct _thread_data_t{
	int id;
	double stuff;
}thread_data_t;

double sh_x;
pthread_mutex_t lock_x;

void *thr_func(void* arg)
{
	thread_data_t* data=(thread_data_t*) arg;
	printf("Hallo from FUNC,thread ID: %d\n",data->id);
	pthread_mutex_lock(&lock_x);
	sh_x+=data->stuff;
	printf("x=%f\n",sh_x);
	pthread_mutex_unlock(&lock_x);
	pthread_exit(NULL);
}

int main()
{
		pthread_t thr[NUM];
		int i,rc;
		thread_data_t thr_data[NUM];
		sh_x=0;
		pthread_mutex_init(&lock_x,NULL);
		for(i=0;i<NUM;i++){
			thr_data[i].id=i;
			thr_data[i].stuff=(i+1)*NUM;
			if(rc=pthread_create(&thr[i],NULL,thr_func,&thr_data[i])){
				fprintf(stderr,"error: pthread_create,rc: %d\n",rc);
				return 0;
			}
		}
		for(i=0;i<NUM;i++)
			pthread_join(thr[i],NULL);
		return 0;
}