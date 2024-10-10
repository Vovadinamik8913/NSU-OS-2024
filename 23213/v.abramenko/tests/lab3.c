#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>


void test_file() {
    FILE* file = fopen("lab3_file", "r");
    if (file == NULL) {
        perror("can`t open file");
    } else {
        printf("opened succesfuly\n");
        fclose(file);
        file = NULL;
    }
}


int main() {
    uid_t uid = getuid();
    uid_t e_uid = geteuid();
    printf("uid: %d\n", uid);
    printf("euid: %d\n", e_uid);

    test_file();

    if(setuid(uid) == -1) {
        perror("setuid failed");
        exit(-1);
    }
    
    e_uid = getuid();
    printf("uid: %d\n", uid);
    printf("euid: %d\n", e_uid);

    test_file();

    exit(0);
}
