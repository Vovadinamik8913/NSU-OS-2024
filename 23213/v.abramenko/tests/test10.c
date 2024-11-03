#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

int main(int argc, char **argv) {

    if (argc < 2){
        fprintf(stderr, "not enough arguments\n");
        exit(-1);
    }

    int status;

    execvp(argv[1], &argv[1]);
    perror("execvp failed");
    exit(127);
}
