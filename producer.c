#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include "pre_define.h"


int main(int argc, char *argv[]) {
    //check arguement number
    if(argc != 5){
        puts("usage: ./prog M(number of data) R(rate) N(number of consumer_num) B(number of buffer)");
        exit(0);
    }

    int data_num = atoi(argv[1]);
    int rate = atoi(argv[2]);
    // rate*=1000;  //us to ms
    int consumer_num = atoi(argv[3]);
    int buff_num = atoi(argv[4]);

    //create shared memory
    int shm_arr[buff_num];
    void *shm_ptr[buff_num];
    key_t key_arr[buff_num];
    for(int i=0;i<buff_num;i++){
       key_arr[i]=KEY+i;
    }

    key_t key_count = KEY+buff_num;
    int shm_count = shmget(key_count, consumer_num*sizeof(int), IPC_CREAT| IPC_EXCL | 0644);    //size is consumer number * integer byte
    if(shm_count == -1){
        perror("count create shared memory failed");
        shm_count = shmget(key_count, 0, 0);     //get already exist shm
        if(shmctl(shm_count, IPC_RMID, NULL) == -1){    //delet shared memory
            perror("delet shm failed");
            exit(EXIT_FAILURE);
        }
        shm_count=shmget(key_count, consumer_num*4, IPC_CREAT| IPC_EXCL | 0644);    //create new shared memory
    }

    void *count_ptr = shmat(shm_count, 0, 0);
    if(count_ptr == -1){
        perror("count shmat failed");
        exit(EXIT_FAILURE);
    }

    for(int i=0;i<buff_num;i++){
        int shm = shmget(key_arr[i], BUF_SIZE, IPC_CREAT | 0644);
        if(shm == -1){
            perror("buffer create shared memory failed");
            shm = shmget(key_arr[i], 0, 0);     //get already exist shm
            if(shmctl(shm, IPC_RMID, NULL) == -1){    //delet shared memory
                perror("delet shm failed");
                exit(EXIT_FAILURE);
            }
            shm=shmget(key_count, BUF_SIZE, IPC_CREAT| IPC_EXCL | 0644);    //create new shared memory
        }
        shm_arr[i] = shm;
        void *ptr = NULL;
        ptr = shmat(shm, 0, 0);
        if(ptr == PTR_ERR){
            perror("shmat failed");
            exit(EXIT_FAILURE);
        }
        shm_ptr[i] = ptr;
    }

    //create delivering to child proc's arg
    char *pass_arg[4];
    pass_arg[0]= CONSUMER_FILE;     //command
    pass_arg[1]= argv[4];       //number of buffer
    pass_arg[3]= NULL;

    //create child process
    pid_t child_pid;
    pid_t child_pid_arr[consumer_num];
    for(int i=0;i<consumer_num;i++){
        char serial_str[10];
        sprintf(serial_str, "%d", i+1);
        pass_arg[2]=serial_str;     //child's serial number

        switch (child_pid = fork())
        {
        case -1:
            perror("create child process failed");
            exit(EXIT_FAILURE);
        case 0:     //child proc
            if(execv(CONSUMER_PATH, pass_arg) == -1){
                perror("exec failed");
                exit(EXIT_FAILURE);
            }
        default:    //parent proc
            child_pid_arr[i]=child_pid;     //record child pid
            break;
        }
    }

    //wait all child process created
    int child_count=0;
    int *p = (int *)count_ptr;
    while(child_count!=consumer_num){
        if(p[child_count]!=1){
            child_count=0;
            continue;
        }
        child_count++;
    }

    //set all child shm record to 0
    for(int i=0;i<consumer_num;i++){
        p[i]=0;
    }

    //write data into shared memory
    int buf_row_index = 0;
    for(int i=0;i<data_num;i++){
        char data[]="This is message ";
        char msg_num[10];
        sprintf(msg_num, "%d", i+1);
        strcat(data, msg_num);
        strcpy(shm_ptr[buf_row_index], data);

        int *p = (int *)count_ptr;
        *p = buf_row_index;         //use first int(4 bytes) to record buffer NO.

        buf_row_index++;
        if(buf_row_index == buff_num)
            buf_row_index = 0;

        for(int i=0;i<consumer_num;i++){    //deliver signal to child then child will receive data
            kill(child_pid_arr[i], SIGUSR1);
        }
        if((i+1)%100==0)
            printf("sent %d data\n", i+1);
        usleep(rate); 
    }

    //terminate child proc
    for(int i=0;i<consumer_num;i++){    
        kill(child_pid_arr[i], SIGINT);
    }

    //wait child terminate
    for(int i=0;i<consumer_num;i++){
        waitpid(child_pid_arr[i], NULL, 0);
    }

    //compute result 
    int result = 0;
    for(int i=0;i<consumer_num;i++){
        result+=p[i];
    }
    double loss_rate = 1 - (double)result/ (data_num * consumer_num);
    printf("M = %d R = %d(ms) N = %d\n", data_num, rate, consumer_num);
    puts("-----------------------------");
    printf("Total message: %d\n", data_num*consumer_num);
    printf("Sum of received messages by all consumers: %d\n", result);
    printf("Loss rate: %lf\n", loss_rate);
    puts("-----------------------------");

    //detach and delete shared memory
    for(int i=0;i<buff_num;i++){
       int unmount_shm = shmdt(shm_ptr[i]);
       if(unmount_shm == -1){
           perror("detached failed");
       }
       int del_shm = shmctl(shm_arr[i], IPC_RMID, NULL);
       if(del_shm == -1){
           perror("delet shared memory failed");
       }
    }
    int unmount_shm = shmdt(count_ptr);
    if(unmount_shm == -1){
        perror("detached failed");
    }
    int del_shm = shmctl(shm_count, IPC_RMID, NULL);
    if(del_shm == -1){
        perror("delet shared memory failed");
    }

    exit(EXIT_SUCCESS);
}
