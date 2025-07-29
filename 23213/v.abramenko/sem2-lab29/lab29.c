#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <termios.h>
#include <aio.h>
#include <errno.h>

#define LINES_PER_SCREEN 25
#define RING_BUFFER_SIZE 8192

typedef struct {
    char* str;
    size_t len;
} string_t;

typedef struct buffer {
    char data[RING_BUFFER_SIZE];
    size_t head;
    size_t tail;
    size_t free;
} buffer_t;

int connection_closed = 0;
int lines_printed = 0;
int is_tty = 0;
struct aiocb *aiocbs[2] = { NULL, NULL };
buffer_t buf = { .head = 0, .tail = 0, .free = RING_BUFFER_SIZE };
struct aiocb client_aiocb;
struct aiocb server_aiocb;
struct termios original;

void end(int code) {
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    exit(code);
}

void parse_url(char* url, string_t* host, string_t* path, int* port) {
    char* start = strstr(url, "://");
    if (start) {
        start += 3;
    } else {
        start = url;
    }
    
    char* port_start = strchr(start, ':');
    char* path_start = strchr(start, '/');
    
    if (port_start && (!path_start || port_start < path_start)) {
        host->len = port_start - start;
        host->str = malloc(host->len + 1);
        if (!host->str) {
            perror("malloc error for host");
            exit(EXIT_FAILURE);
        }
        strncpy(host->str, start, host->len);
        host->str[host->len] = '\0';
        char* port_end = path_start ? path_start : start + strlen(start);
        char port_str[6] = {0};
        size_t port_len = port_end - (port_start + 1);
        if (port_len > 5) {
            fprintf(stderr, "Invalid port number\n");
            exit(EXIT_FAILURE);
        }
        strncpy(port_str, port_start + 1, port_len);
        *port = atoi(port_str);
        if (*port <= 0 || *port > 65535) {
            fprintf(stderr, "Invalid port number: %s\n", port_str);
            exit(EXIT_FAILURE);
        }
    } else {
        port_start = path_start ? path_start : start + strlen(start);
        host->len = port_start - start;
        host->str = malloc(host->len + 1);
        if (!host->str) {
            perror("malloc error for host");
            exit(EXIT_FAILURE);
        }
        strncpy(host->str, start, host->len);
        host->str[host->len] = '\0';
        *port = 80;
    }

    if (path_start) {
        path->len = strlen(path_start);
        path->str = malloc(path->len + 1);
        if (!path->str) {
            perror("malloc error for path");
            exit(EXIT_FAILURE);
        }
        strcpy(path->str, path_start);
    } else {
        path->len = 1;
        path->str = malloc(2);
        if (!path->str) {
            perror("malloc error for path");
            exit(EXIT_FAILURE);
        }
        path->str[0] = '/';
        path->str[1] = '\0';
    }
}

void non_canonical_mode() {
    tcgetattr(STDIN_FILENO, &original);
    struct termios ttystate = original;
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

void print(size_t start, size_t length) {
    size_t start_index = (buf.head + start) % RING_BUFFER_SIZE;
    size_t contiguous = RING_BUFFER_SIZE - start_index;

    if (length <= contiguous) {
        write(STDOUT_FILENO, &buf.data[start_index], length);
    } else {
        write(STDOUT_FILENO, &buf.data[start_index], contiguous);
        write(STDOUT_FILENO, buf.data, length - contiguous);
    }
}

void process() {
    size_t bytes_processed = 0;
    size_t line_beginning = 0;
    size_t bytes_to_process = RING_BUFFER_SIZE - buf.free;

    while (bytes_processed < bytes_to_process && lines_printed != LINES_PER_SCREEN) {
        if (!is_tty) {
            lines_printed = 0;
        }

        char c = buf.data[(buf.head + bytes_processed) % RING_BUFFER_SIZE];
        bytes_processed++;

        if (c == '\n') {
            lines_printed++;
            size_t line_length = bytes_processed - line_beginning;
            print(line_beginning, line_length);
            line_beginning = bytes_processed;
        }
    }

    if (bytes_processed == bytes_to_process
        && line_beginning < bytes_to_process) {
        size_t line_length = bytes_to_process - line_beginning;
        print(line_beginning, line_length);
    }

    buf.head = (buf.head + bytes_processed) % RING_BUFFER_SIZE;
    buf.free += bytes_processed;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s URL\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    struct sockaddr_in server_addr;
    string_t host, path;
    int port;
    int fd;
    parse_url(argv[1], &host, &path, &port);
    
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    struct hostent* server = gethostbyname(host.str);
    if (server == NULL) {
        herror("gethostbyname error");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    server_addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr*)&server_addr, sizeof(server_addr))) {
        perror("connect error");
        exit(EXIT_FAILURE);
    }

    char request[host.len + path.len + 50];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path.str, host.str); 
    if (write(fd, request, strlen(request)) < 0) {
        perror("write error");
        exit(EXIT_FAILURE);
    }

    if (isatty(STDOUT_FILENO)) {
        is_tty = 1;
    }
    
    non_canonical_mode();

    char term_buf;
    memset(&client_aiocb, 0, sizeof(struct aiocb));
    client_aiocb.aio_fildes = STDIN_FILENO;
    client_aiocb.aio_buf = &term_buf;
    client_aiocb.aio_nbytes = 1;
    client_aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;

    memset(&server_aiocb, 0, sizeof(struct aiocb));
    server_aiocb.aio_fildes = fd;
    server_aiocb.aio_buf = buf.data;
    server_aiocb.aio_nbytes = RING_BUFFER_SIZE;
    server_aiocb.aio_sigevent.sigev_notify = SIGEV_NONE;

    if (aio_read(&server_aiocb) < 0) {
        perror("server aio_read error");
        end(EXIT_FAILURE);
    }
    if (aio_read(&client_aiocb) < 0) {
        perror("client aio_read error");
        end(EXIT_FAILURE);
    }

    aiocbs[0] = NULL; 
    aiocbs[1] = &server_aiocb;

    while (1) {
        process();
        if (is_tty && lines_printed == LINES_PER_SCREEN && aiocbs[0] == NULL) {
            const char *message = "-- Press space to continue --\n";
            write(STDOUT_FILENO, message, strlen(message));
            aiocbs[0] = &client_aiocb;
        }
        if (buf.free > 0 && !connection_closed && aiocbs[1] == NULL) {
            if (buf.tail >= buf.head) {
                size_t space_to_end = RING_BUFFER_SIZE - buf.tail;
                server_aiocb.aio_buf = &buf.data[buf.tail];
                server_aiocb.aio_nbytes = space_to_end;
            } else {
                server_aiocb.aio_buf = &buf.data[buf.tail];
                server_aiocb.aio_nbytes = buf.free;
            }
            if (aio_read(&server_aiocb) != 0) {
                perror("server aio_read error");
                end(EXIT_FAILURE);
            }
            aiocbs[1] = &server_aiocb;
        } 
        if (aiocbs[0] == NULL && aiocbs[1] == NULL) {
            break;
        }
        aio_suspend((const struct aiocb *const *)aiocbs, 2, NULL);
        if (aiocbs[0] != NULL && aio_error(&client_aiocb) == 0) {
            ssize_t bytes_read = aio_return(&client_aiocb);
            aiocbs[0] = NULL;
            if (bytes_read > 0) {
                char input_char = *(char*)client_aiocb.aio_buf;
                if (input_char == ' ') {
                    lines_printed = 0;
                } else {
                    aiocbs[0] = &client_aiocb;
                }
                if (aio_read(&client_aiocb) != 0) {
                    perror("client aio_read error");
                    end(EXIT_FAILURE);
                }
            } else {
                if (bytes_read < 0) {
                    perror("client aio_return error");
                }
                end(EXIT_FAILURE);
            }
        }
        if (aiocbs[1] != NULL && aio_error(&server_aiocb) == 0) {
            ssize_t bytes_read = aio_return(&server_aiocb);
            if (bytes_read > 0) {
                aiocbs[1] = NULL;
                buf.tail = (buf.tail + bytes_read) % RING_BUFFER_SIZE;
                buf.free -= bytes_read;
            } else if (bytes_read == 0) {
                connection_closed = 1;
                aiocbs[1] = NULL;
            } else {
                perror("server aio_return error");
                end(EXIT_FAILURE);
            }
        }
    }

    if (is_tty && connection_closed) {
        const char *message = "Connection closed by server.\n";
        write(STDOUT_FILENO, message, strlen(message));
    }
    end(EXIT_SUCCESS);
}
