#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

#define BUFSIZE 4096
#define MAX_CONNECTIONS 51
#define MAX_FD 1024

typedef struct info {
    int fd;
    ssize_t len;
    char buf[BUFSIZE];
} info_t;

typedef struct connection {
    info_t client;
    info_t server;
} connection_t;

void signal_handler(int sig) {
    _exit(EXIT_SUCCESS);
}

void info_init(info_t* a, int fd) {
    a->fd = fd;
    a->len = 0;
    memset(a->buf, 0, BUFSIZE);
}

int start_proxy(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socker error");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, MAX_CONNECTIONS) < 0) {
        perror("listen error");
        exit(EXIT_FAILURE);
    }
    return fd;
}

int remote_connect(struct hostent* host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket error");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr = *(struct in_addr*)host->h_addr_list[0];

    if ((connect(fd, (struct sockaddr *)&addr, sizeof(addr))) == -1) {
        perror("connect error");
        close(fd);
        return -1;
    }

    return fd;
}

int proccess_data(info_t from, info_t to) {

    from.len = recv(from.fd, from.buf, BUFSIZE, 0);

    if (from.len <= 0) {
        if (from.len == 0) {
            fprintf(stderr, "Disconnected\n");
        } else {
            perror("recv error");
        }
        return -1;
    }

    if ((send(to.fd, from.buf, from.len, 0)) <= 0) {
        perror("send error");
        return -1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s  'listen port' 'host' 'remote port'\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int listen_port = atoi(argv[1]);
    if (listen_port < 1) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    
    struct hostent* host = gethostbyname(argv[2]);
    if (host == NULL) {
        herror("gethostbyname error");
        exit(EXIT_FAILURE);
    }

    int remote_port = atoi(argv[3]);
    if (remote_port < 1) {
        fprintf(stderr, "Invalid remote port number: %s\n", argv[3]);
        exit(EXIT_FAILURE);
    }

    int server = start_proxy(listen_port);

    signal(SIGINT, signal_handler);

    int max_size = MAX_CONNECTIONS * 2 + 1;
    int cur_size = 0;
    struct pollfd fds[max_size];
    connection_t connections[MAX_CONNECTIONS];

    fds[cur_size].fd = server;
    fds[cur_size].events = POLLIN;
    cur_size++;

    while (1) {
        if (poll(fds, cur_size, -1) == -1) {
            perror("poll error");
            break;
        }
        for (int i = 0; i < cur_size; i++) {
            if (fds[i].revents & POLLIN) {
                if (i == 0) {
                    int client_fd = accept(server, NULL, NULL);
                    if (client_fd == -1) {
                        continue;
                    }
                
                    int server_fd = remote_connect(host, remote_port);
                    if (server_fd == -1) {
                        close(client_fd);
                        continue;
                    }

                    if (cur_size >= max_size) {
                        fprintf(stderr, "Too many connections\n");
                        close(client_fd);
                        close(server_fd);
                        continue;
                    }

                    fds[cur_size].fd = client_fd;
                    fds[cur_size].events = POLLIN;
                    info_init(&connections[(cur_size - 1) / 2].client, client_fd);
                    cur_size++;
                    fds[cur_size].fd = server_fd;
                    fds[cur_size].events = POLLIN;
                    info_init(&connections[(cur_size - 1) / 2].server, server_fd);
                    cur_size++;
                } else {
                    int res = 0;
                    int isClient = (i % 2 == 1);
                    int id = (i - 1) / 2;
                    if (isClient) {
                        res = proccess_data(connections[id].client, connections[id].server);
                    } else {
                        res = proccess_data(connections[id].server, connections[id].client);
                    }

                    if (res != 0) {
                        close(connections[id].client.fd);
                        close(connections[id].server.fd);
                        int last = (cur_size - 1) / 2; 
                        if (isClient) {
                            fds[i] = fds[cur_size - 2];
                            fds[i + 1] = fds[cur_size - 1];
                        } else {
                            fds[i - 1] = fds[cur_size - 2];
                            fds[i] = fds[cur_size - 1];
                        }
                        connections[id] = connections[last];
                        fds[cur_size - 2].fd = -1;
                        fds[cur_size - 2].events = 0;
                        fds[cur_size - 1].fd = -1;
                        fds[cur_size - 1].events = 0;
                        cur_size -= 2;
                        i--;
                    }
                    
                }
            }
        }
    }
    exit(EXIT_SUCCESS);
}
