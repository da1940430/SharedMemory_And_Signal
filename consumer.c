#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <signal.h>
#include "pre_define.h"
void handler(int);

void *shm_ptr=NULL;
int *p;
int serial, count=0;
void *data;
int main(int argc, char *argv[]){
    // printf("This is child NO.%s\n", argv[2]);
    data = malloc(BUF_SIZE);
    int buff_num = atoi(argv[1]);
    serial = atoi(argv[2]);
    key_t key_arr[buff_num];
    for(int i=0;i<buff_num;i++){
       key_arr[i]=KEY+i;
    }

    key_t key_count = KEY+buff_num;
    int shm_count = shmget(key_count, 0, 0);
    void *count_ptr = shmat(shm_count, 0, 0);
    if(count_ptr == -1){
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    //mount shared memory
    int shm_arr[buff_num];
    void *shm_ptr_arr[buff_num];
    for(int i=0;i<buff_num;i++){
        // arr[i] = atoi(argv[i]);
        shm_arr[i] = shmget(key_arr[i], 0, 0);
        if(shm_arr[i] == -1){
            perror("shmget failed");
            exit(EXIT_FAILURE);
        }
        void *ptr_temp = NULL;
        ptr_temp = shmat(shm_arr[i], 0, 0);
        if(ptr_temp == PTR_ERR){
            perror("shmat failed");
            exit(EXIT_FAILURE);
        }
        shm_ptr_arr[i] = ptr_temp;
    }

    //create block signal
    struct sigaction act;
    act.sa_handler = &handler;
    act.sa_flags = 0;
    sigfillset(&act.sa_mask);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGINT, &act, NULL);

    //signal set
    sigset_t empty;
    sigemptyset(&empty);
    //block until receive signal
    int buff_index=0;

    //ready to receive data
    p = (int *)count_ptr;
    p[serial - 1]++;
    while(1){
        shm_ptr=shm_ptr_arr[buff_index];
        sigsuspend(&empty);
        buff_index++;
        if(buff_index == buff_num)
            buff_index=0;
    }
}

void handler(int sig){
    if(sig == SIGUSR1){
        // puts("get data");
        memcpy(data, (void *)p, BUF_SIZE);
        count++;
    }
    if(sig == SIGINT){
        //when all data has delivered and receive SIGINT, compute received number of data and write into shm
        p[serial - 1] = count;  
        exit(EXIT_SUCCESS);
    }
}