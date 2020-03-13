#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

pthread_cond_t      cond  = PTHREAD_COND_INITIALIZER;
pthread_mutex_t     mutex = PTHREAD_MUTEX_INITIALIZER;
int interrupted;

#define WAIT_TIME_SECONDS 20
void *event_loop (void *arg)
{
    struct timespec   ts;
    struct timeval    tv;

again:
    gettimeofday(&tv, NULL);

    ts.tv_sec  = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
    ts.tv_sec += WAIT_TIME_SECONDS;

    pthread_mutex_lock(&mutex);
    while (!interrupted) {
        int rc = pthread_cond_timedwait(&cond, &mutex, &ts);
        if (rc == ETIMEDOUT) {
            printf("Timed out!\n");
            pthread_mutex_unlock(&mutex);
            goto again;
        }
    }
    printf("Woke up!\n");
    interrupted = 0;
    pthread_mutex_unlock(&mutex);
    goto again;
}

int main(void) {
    pthread_t thread;
    pthread_create(&thread, NULL, event_loop, NULL);
    while (getchar() != EOF) {
        pthread_mutex_lock(&mutex);
        interrupted = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);
    }
}

