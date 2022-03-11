#include <ucontext.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>
#include "green.h"

#define FALSE 0
#define TRUE 1

#define STACK_SIZE 4096

#define PERIOD 100

static ucontext_t main_cntx = {0};
static green_t main_green = {&main_cntx, NULL, NULL, NULL, NULL, FALSE};

static green_t *running = &main_green;

struct green_t *ready_queue = NULL;

static sigset_t block;

void timer_handler(int); 

static void init() __attribute__((constructor));

void init()
{
    getcontext(&main_cntx);

    //Timer initialization
    sigemptyset(&block);
    sigaddset(&block, SIGVTALRM);

    struct sigaction act = {0};
    struct timeval interval;
    struct itimerval period;

    act.sa_handler = timer_handler;
    assert(sigaction(SIGVTALRM, &act, NULL) == FALSE);
    interval.tv_sec = 0;
    interval.tv_usec = PERIOD;
    period.it_interval = interval;
    period.it_value = interval;
    setitimer(ITIMER_VIRTUAL, &period, NULL);
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
    //Set up the new context and stack for the new thread.
    ucontext_t *cntx = (ucontext_t *)malloc(sizeof(ucontext_t));
    getcontext(cntx);

    void *stack = malloc(STACK_SIZE);

    cntx->uc_stack.ss_sp = stack;
    cntx->uc_stack.ss_size = STACK_SIZE;
    makecontext(cntx, green_thread, 0);

    //Set up the new thread.
    new->context = cntx;
    new->fun = fun;
    new->arg = arg;
    new->next = NULL;
    new->join = NULL;
    new->retval = NULL;
    new->zombie = FALSE;

    // add new to the ready queue
    sigprocmask(SIG_BLOCK, &block, NULL);
    enqueue(&ready_queue, new);
    sigprocmask(SIG_UNBLOCK, &block, NULL);

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
    sigprocmask(SIG_BLOCK, &block, NULL);
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
    sigprocmask(SIG_UNBLOCK, &block, NULL);
    return 0;
}

//      # # # # # # # # # #
//      # # # # # # # # # #
//      #  3. Conditions  #
//      # # # # # # # # # #
//      # # # # # # # # # #

//Initialize a green condition variable
void green_cond_init(green_cond_t *cond)
{
    sigprocmask(SIG_BLOCK, &block, NULL);
    cond->queue = NULL;
    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

//Suspend the current thread on the condition
void green_cond_wait(green_cond_t *cond)
{
    sigprocmask(SIG_BLOCK, &block, NULL);

    //Copying the process that is currently running
    green_t *susp = running;
    assert(susp != NULL);

    //Adding the current process to the list of currently suspended processes
    enqueue(&cond->queue, susp);

    //Preparing the next thread to be used.
    green_t *next = dequeue(&ready_queue);
    assert(next != NULL);

    //Setting the next thread to be used as the running thread.
    running = next;

    //Swapping the context. This will save the current state and continue execution on next->context.
    swapcontext(susp->context, next->context);

    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

//move the first suspended variable to the ready queue
void green_cond_signal(green_cond_t *cond)
{
    sigprocmask(SIG_BLOCK, &block, NULL);
    //Do not do anything if the queue is empty
    if(cond->queue == NULL)
    {
        return;
    }
    //returning a previously suspended thread and then queueing it up to be used again.
    green_t *thread = dequeue(&cond->queue);
    enqueue(&ready_queue, thread);

    sigprocmask(SIG_UNBLOCK, &block, NULL);
}

//      # # # # # # # # # #
//      # # # # # # # # # #
//      # # # 4. Timer  # #
//      # # # # # # # # # #
//      # # # # # # # # # #

void timer_handler(int sig)
{
    green_t *susp = running;

    //add the running to the ready queue
    enqueue(&ready_queue, susp);
    //find the next thread for execution
    green_t *next = dequeue(&ready_queue);
    running = next;
    swapcontext(susp->context, next->context);
}