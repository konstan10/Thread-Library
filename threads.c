#include "ec440threads.h"
#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#define MAX_THREADS 128
#define THREAD_STACK_SIZE 32767

#define JB_RBX 0
#define JB_RBP 1    //base pointer
#define JB_R12 2    //pointer to function or routine
#define JB_R13 3    //value of *arg
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6    //stack pointer
#define JB_PC  7    //program counter

static int start = 0;
static pthread_t running_tid = 0;

enum ts{
    READY, RUNNING, EXITED, EMPTY
};

struct tcb{
    pthread_t tid;
    jmp_buf regs;
    void* stackptr;
    enum ts status;
};

struct tcb thread_table[MAX_THREADS];
struct sigaction sig_handler;

void schedule() {
    pthread_t chosen_tid;
    int saved = 0;
    if(thread_table[running_tid].status == RUNNING){
        thread_table[running_tid].status = READY;
        saved = setjmp(thread_table[running_tid].regs);
    }
    chosen_tid = running_tid;
    int i;
    for(i = 0; i < MAX_THREADS; i++){
        chosen_tid++;
        if(chosen_tid == MAX_THREADS){
            chosen_tid = 0;
        }
        if(thread_table[chosen_tid].status == READY){
            break;
        }
    }
    if(saved == 0){
        running_tid = chosen_tid;
        thread_table[running_tid].status = RUNNING;
        longjmp(thread_table[running_tid].regs, 1);
    }
}
void init(){
    start = 1;
    int i;
    thread_table[0].status = RUNNING;
    for(i = 1; i < MAX_THREADS; i++){
        thread_table[i].status = EMPTY;
        thread_table[i].tid = i;
    }
	ualarm(50000, 50000);

	sigemptyset(&sig_handler.sa_mask);
	sig_handler.sa_handler = &schedule;
	sig_handler.sa_flags = SA_NODEFER;
	sigaction(SIGALRM, &sig_handler, NULL);
}
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,void *(*start_routine) (void *), void *arg){
    if(start == 0){       //runs only at the beginning
        init();
    }
    pthread_t curr_tid = 1;
    int i;
    for(i = 1; i < MAX_THREADS; i++){
        if(thread_table[i].status == EMPTY || thread_table[i].status == EXITED){
            curr_tid = i;
            break;
        }
    }
    if(curr_tid == MAX_THREADS){
        perror("No threads are available");
        return -1;
    }
    *thread = curr_tid;                                                                           //set thread to current thread                                                
    thread_table[curr_tid].tid = curr_tid;                                                        //set thread id                 
    thread_table[curr_tid].stackptr = (unsigned long int *) ((char*)malloc(THREAD_STACK_SIZE) + THREAD_STACK_SIZE - sizeof(unsigned long int));  //allocate new stack and point to top of stack minus 4
    thread_table[curr_tid].status = READY;  
    unsigned long int *exitptr = thread_table[curr_tid].stackptr;                                 //set exit ptr to stack ptr
    *exitptr = (unsigned long int)pthread_exit;                                                   //have exit ptr point to pthread_exit

    setjmp(thread_table[curr_tid].regs);                                                          //save current environment into buffer
    thread_table[curr_tid].regs[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk);  //set program counter to mangled start_thunk function
    thread_table[curr_tid].regs[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)exitptr);     //set stack pointer to mangled address of pthread_exit
    thread_table[curr_tid].regs[0].__jmpbuf[JB_R12] = (unsigned long int)start_routine;           //set R12 to start routine
    thread_table[curr_tid].regs[0].__jmpbuf[JB_R13] = (unsigned long int)arg;                     //set R13 to argument
                   
    return 0;

}
pthread_t pthread_self(void){
	return running_tid;
}
void pthread_exit(void *value_ptr)
{   
    thread_table[running_tid].status = EXITED;
    int i;
    int threads_remaining = 0;
    //free(thread_table[running_tid].stackptr);
    for(i = 0; i < MAX_THREADS; i++){
        if(thread_table[i].status == READY){
            threads_remaining = 1;
            break;
        }
    }
    if(threads_remaining){
        schedule();
    }
    exit(0);
}
