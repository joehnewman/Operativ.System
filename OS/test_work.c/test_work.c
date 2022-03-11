#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "green.h"


volatile int count = 0;
int flag = 0;
static int numThreads = 0;

typedef struct args {int id; int inc;} args;

green_cond_t cond;
green_mutex_t mutex;
pthread_cond_t condP;
pthread_mutex_t mutexP;

void *test1(void *arg) {
    int i = *(int*)arg;
    int loop = 100;
    while(loop > 0) {
        printf("thread %d: %d\n", i , loop);
        loop--;
        green_yield();
    } 
}

void *test2(void *arg) {
    int id = *(int *)arg;
    int loop = ((args*)arg)->inc;
    while(loop > 0) {
        if(flag == id){
            // printf("thread %d: %d\n", id, loop);
            loop--;
            count++;
            flag = (id+1) % numThreads;
            green_cond_signal(&cond);
        } else {
            green_cond_wait(&cond, NULL);
        }
    }
}


// green_mutex_init(mutex);
void *test3(void *arg) {
    int id = ((args*)arg)->id;
    int loop = ((args*)arg)->inc;
    while(loop > 0) {
        green_mutex_lock(&mutex);
        if(flag != id){
            green_mutex_unlock(&mutex);
            green_cond_wait(&cond, NULL);
            green_mutex_lock(&mutex);
        } 
            flag = (id+1) % 4;
            green_cond_signal(&cond);
            green_mutex_unlock(&mutex);
            count++;
            loop--;
    }
}

void *test4(void *arg) {
    int id = ((args*)arg)->id;
    int loop = ((args*)arg)->inc;
    while(loop > 0) {
        green_mutex_lock(&mutex);
        if(flag != id){
            green_cond_wait(&cond, &mutex);
        } 
            flag = (id+1) % 4;
            green_cond_signal(&cond);
            green_mutex_unlock(&mutex);
            count++;
            loop--;
    }
}
void *test4P(void *arg) {
    int id = ((args*)arg)->id;
    int loop = ((args*)arg)->inc;
    while(loop > 0) {
        pthread_mutex_lock(&mutexP);
        if(flag != id){
            pthread_cond_wait(&condP, &mutexP);
        } 
            flag = (id+1) % numThreads;
            pthread_cond_signal(&condP);
            pthread_mutex_unlock(&mutexP);
            count++;
            loop--;
    }
}

void* test5(void* arg) {
    int id = ((args*)arg)->id;
    int loop = 4;
    while (loop > 0) {
        green_mutex_lock(&mutex);
        while (flag != id) {
            green_cond_wait(&cond, &mutex);
        }
        flag = (id + 1) % 4;
        green_cond_signal(&cond);
        green_mutex_unlock(&mutex);
        loop--;
    }   
}


int main(int argc, int *argv[]) {

    if(argc != 4) {
        printf("usage: testloop <total> <threads> <type>\n");
        exit(0);
    }

    int tpe = atoi(argv[3]); 
    int n = atoi(argv[2]);
    numThreads = n;
    int inc = (atoi(argv[1]) / n);

    pthread_t *p_threads = malloc(n * sizeof(pthread_t));
    green_t *g_threads = malloc(n*sizeof(green_t));
    args *args = malloc(n*sizeof(args));

    struct timespec t_start, t_stop;

    for (int i = 0; i < n; i++)
    {
        args[i].inc = inc; 
        args[i].id = i; 
    }
    if(tpe == 0) {
        green_cond_init(&cond);
        green_mutex_init(&mutex);

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_start);

        for (int i = 0; i < n; i++)
        {
            green_create(&g_threads[i], test3, &args[i]);// create a thread on the heap
        }
        for (int i = 0; i < n; i++)
        {
            green_join(&g_threads[i], NULL); // start thread 
        }
        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_stop);
    } else {
        pthread_cond_init(&condP, NULL);
        pthread_mutex_init(&mutexP, NULL);

        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_start);

        for (int i = 0; i < n; i++)
        {
            pthread_create(&p_threads[i], NULL, test4P, &args[i]);// create a thread on the heap
        }
        for (int i = 0; i < n; i++)
        {
            pthread_join(p_threads[i], NULL); // start thread 
        }
        clock_gettime(CLOCK_MONOTONIC_COARSE, &t_stop);
    }
    

    long wall_sec = t_stop.tv_sec - t_start.tv_sec;
    long wall_nsec = t_stop.tv_nsec - t_start.tv_nsec;
    long wall_msec = (wall_sec *1000) + (wall_nsec / 1000000);
    
    printf("%ld\t\t%d\t\t%d\n", wall_msec, count, numThreads*inc);

    return 0;
}

