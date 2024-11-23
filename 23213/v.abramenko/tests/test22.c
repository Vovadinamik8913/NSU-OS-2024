#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <termios.h>

void bring_to_foreground(pid_t pid, int terminal_fd) {
    // Устанавливаем группу процессов
    if (tcsetpgrp(terminal_fd, pid) == -1) {
        perror("tcsetpgrp");
        exit(EXIT_FAILURE);
    }

    // Отправляем сигнал SIGCONT для возобновления процесса
    if (kill(pid, SIGCONT) == -1) {
        perror("kill");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {

    pid_t pid = getpid();
    const char *terminal_path = argv[1];

    // Открываем терминал
    int terminal_fd = open(terminal_path, O_RDWR);
    if (terminal_fd == -1) {
        perror("Ошибка открытия терминала");
        return EXIT_FAILURE;
    }

    // Переводим процесс на передний план
    bring_to_foreground(pid, terminal_fd);

    // Закрываем дескриптор терминала
    close(terminal_fd);

    return EXIT_SUCCESS;
}
