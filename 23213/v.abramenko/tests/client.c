#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <errno.h>

#define BUF_SIZE 1024

char *socket_path = "./socket";

int main(int argc, char *argv[]) {
    struct sockaddr_un addr;
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connection failed");
        close(fd);
        exit(-1);
    }

    char buf[BUF_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
        if (write(fd, buf, sizeof(buf)) == -1) {
            perror("write");
            close(fd);
            exit(-1);
        }
    }

    if (bytes_read == -1) {
        perror("read failed");
        close(fd);
        exit(-1);
    }

    close(fd);
    exit(0);
} 
