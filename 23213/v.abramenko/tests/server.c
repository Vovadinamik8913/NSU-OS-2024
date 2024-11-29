#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <aio.h>
#include <signal.h>
#include <setjmp.h>
#include <siginfo.h>

#define MAX_CLIENTS 10
#define BUF_SIZE 1024

char *socket_path = "./socket";

void close_handler(int sig) {
	unlink(socket_path);
	_exit(0);
}

struct sigjmp_buf toexit;

void sigiohandler(int signo, siginfo_t* siginfo, void* context){
    if (signo != SIGIO || siginfo.si_signo != SIGIO){
        return;
    }

    struct aiocb *request = siginfo->si_value.sival_ptr;
    if (aio_error(request) == 0){
        int rc = aio_return(request);
        if (rc <= 0) {
            if (rc == -1) {
                perror("return failed");
            }
            char* buffer = (char*) request->aio_buf;
            free(buffer);
            close(request->aio_fildes);
            free(request);
        } else {
            char* buf = (char*) request->aio_buf;
            buf[rc] = 0;
            for (int j = 0; j < rc; j++) {
                putchar(toupper((unsigned char)buf[j]));
            }
            aio_read(request);
        }
        siglongjmp(toexit, 1);
    }
}


int main() {
    struct sockaddr_un addr;
    int fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket failed");
        exit(-1);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("binding failed");
        close(fd);
        exit(-1);
    }

    if (listen(fd, MAX_CLIENTS) == -1) {
        perror("listen failed");
        unlink(socket_path);
        close(fd);
        exit(-1);
    }

    signal(SIGINT, close_handler);

    struct sigaction sigiohandleraction;
    sigemptyset(&sigiohandleraction.sa_mask);
    sigaddset(&sigiohandleraction.sa_mask, SIGIO);

    sigiohandleraction.sa_sigaction = SIGIO_handler;
    sigiohandleraction.sa_flags = SA_SIGINFO;
    sigaction(SIGIO, &sigiohandleraction, NULL);


    int cl;
    while (1) {
        if ((cl = accept(fd, NULL, NULL)) == -1){
            perror("accept failed");
            continue;
        }
        struct aiocb* request = malloc(sizeof(struct aiocb));
        request->aio_fildes = cl;
        request->aio_offset = 0;
        requests->aio_buf = malloc(BUF_SIZE * sizeof(char));
        requests->aio_nbytes = BUF_SIZE - 1;
        requests->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
        requests->aio_sigevent.sigev_signo = SIGIO;
        aio_read(request);
    }
}
