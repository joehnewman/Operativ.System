#include <ucontext.h>
#include <stdlib.h>
#include <assert.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};

static green_t *running = &main_green;

struct green_t *ready_queue = NULL;


static void init() __attribute__((constructor));

void init()
{
    getcontext(&main_cntx);
}

void enqueue(green_t **list, green_t *thread)
{
    if(*list == NULL)
    {
        *list = thread;
    }
    else
    {
        green_t *susp = *list;
        while(susp->next != NULL)
        {
            susp = susp->next;

        }
        susp->next = thread;
    }
}

green_t *dequeue(green_t **list)
{
    if(*list == NULL)
    {
        return NULL;
    }
    else
    {
        green_t *thread = *list;
        *list = (*list)->next;
        thread->next = NULL;
        return thread;
    }
}

void green_thread()
{
    green_t *this = running;

    void *result = (*this->fun)(this->arg);
    // place waiting (joining) thread in ready queue
    enqueue(&ready_queue, this->join);

    // save result of execution
    this->retval = result;

    // we're a zombie
    this->zombie = TRUE;
    // find the next thread to run
    green_t *next = dequeue(&ready_queue);

    running = next;
    setcontext(next->context);
}


int green_create(green_t *new, void *(*fun)(void *), void *arg)
{
    ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(cntx);

    void *stack = malloc(STACK_SIZE);

    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;
    makecontext(cntx, green_thread, 0);

    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->retval = NULL;
    new->zombie = FALSE;

    // add new to the ready queue
   
    enqueue(&ready_queue, new);

    return 0;
}

int green_yield()
{
    green_t *susp = running;
    // add susp to ready queue
    enqueue(&ready_queue, susp);
    // select the next thread for execution
    green_t *next = dequeue(&ready_queue);

    running = next;
    //swap context
    swapcontext(susp->context, next->context);
    return 0;
}

int green_join (green_t *thread , void **res) 
{
    if(!thread->zombie) 
    {
        green_t *susp = running;
        // add as joining thread
        thread->join = susp;
        //select the next thread for execution
        green_t *next = dequeue(&ready_queue); 
        running = next;
        swapcontext(susp->context, next->context) ;
    }
    // collect result
    if(thread->retval != NULL)
    {
        *res = thread->retval;
    }
    // free context
    free(thread->context);
    return 0;
}