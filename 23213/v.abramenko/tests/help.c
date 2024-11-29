#include <sys/socket.h>

char* socket_path = "./socket";

int main() {
    unlink(socket_path);
    return 0;
}
