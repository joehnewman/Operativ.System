#include "green.h"
#include <stdio.h>

green_mutex_t mutex;
void *test(void *arg)
{
    int id = *(int*)arg;
    int loop = 4;

    while(loop >= 0)
    {
        green_mutex_lock(&mutex);
        printf("Thread %d has locked on loop %d\n", id, loop);
        --loop;
        printf("Thread %d is unlocking\n", id);
        green_mutex_unlock(&mutex);
    }
}

int main()
{

    green_mutex_init(&mutex);
    green_t g0, g1;
    int a0 = 0;
    int a1 = 1;
    green_create(&g0, test, &a0);
    green_create(&g1, test, &a1);
    green_join(&g0, NULL);
    green_join(&g1, NULL);
    printf("done\n");
    return 0;
}