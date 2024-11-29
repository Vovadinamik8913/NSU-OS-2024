#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <aio.h>

#define MAX_CLIENTS 10
#define BUF_SIZE 1024

char *socket_path = "./socket";

void handle_sigint(int sig) {
	unlink(socket_path);
	_exit(0);
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

    signal(SIGINT, handle_sigint);

    int cl;
    struct aiocb requests[MAX_CLIENTS];
    const struct aiocb* view[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        view[i] = &requests[i];
    }
    int cnt_requests = 0;
    while (1) {
        int cl = accept(fd, NULL, NULL);
        if (cl == -1){
            perror("accept failed");
        } else {
            requests[cnt_requests].aio_fildes = cl;
            requests[cnt_requests].aio_offset = 0;
            requests[cnt_requests].aio_buf = malloc(BUF_SIZE);
            requests[cnt_requests].aio_nbytes = BUF_SIZE - 1;
            requests[cnt_requests].aio_sigevent.sigev_notify = SIGEV_NONE;
            aio_read(&requests[cnt_requests]);
            cnt_requests++;
        }
        
    
        aio_suspend(view, cnt_requests, NULL);
        for (int i = 0; i < cnt_requests; i++) {
            ssize_t rc = aio_return(&requests[i]);
            if (rc <= 0) {
                if(rc == -1) {
                    if (errno == EINPROGRESS) {
                        aio_read(&requests[i]);
                        continue;
                    }
                    perror("return failed");
                }
                char* buf = (char*)requests[i].aio_buf;
                free(buf);
                close(requests[i].aio_fildes);
                requests[i] = requests[cnt_requests - 1];
                requests[cnt_requests - 1].aio_fildes = -1;
                requests[cnt_requests - 1].aio_buf = NULL;
                cnt_requests--;
                i--;
            } else {
                char* buf = (char*)requests[i].aio_buf;
                buf[rc] = 0;
                for (int j = 0; j < rc; j++) {
                    putchar(toupper((unsigned char)buf[j]));
                }
                aio_read(&requests[i]);
            }
        }
        

        
    }
}
