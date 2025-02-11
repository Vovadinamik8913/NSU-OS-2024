#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/wait.h>

char* parent = "/sync_parent";
char* child =  "/sync_child";

int main(int argc, char** argv) {
    sem_t *sem_parent = sem_open(parent, O_CREAT | O_EXCL, 0644, 1);
    if (sem_parent == NULL) {
        perror("sem_ open fail");
        exit(-1);
    }
    
    sem_t *sem_child = sem_open(child, O_CREAT | O_EXCL, 0644, 0);
    if (sem_child == NULL) {
        perror("sem_ open fail");
        exit(-1);
    }

    sem_unlink(child);
    sem_unlink(parent);

    pid_t pid;
    pid = fork();
    switch (pid)
    {
    case -1:
        perror("fork failed");
        exit(-1);
    case 0:
        for (int i = 0; i < 10; i++)
        {
            sem_wait(sem_child);
            printf("child\n");
            sem_post(sem_parent);
        }
        exit(0);
    default:
        for (int i = 0; i < 10; i++)
        {
            sem_wait(sem_parent);
            printf("parent\n");
            sem_post(sem_child);
        }
        if(wait(NULL) == -1) {
            perror("wait failed");
            exit(-1);
        }
        exit(0);
    }
}