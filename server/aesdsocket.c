// I used google and AI here pretty liberally. I'm a C# dev not a C dev.
// It would be impractical to document every use, but I primarly would ask AI for things like "write a function that does X" or "how do I do Y, provide an example" and then write *my own* code, using the response/example as reference.
// I did not just copy-and-paste.

#include "aesdsocket.h"

#define WRITE_PATH "/var/tmp/aesdsocketdata"
#define PORT 9000
# define STRERROR = strerror(errno)


static volatile sig_atomic_t stop_signal = 0;




int main(int argc, char *argv[]) {
    openlog(argv[0], NULL, LOG_USER);

    // signal link:
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT,  &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "sigaction: %s", STRERROR);
        closelog(); return -1;
    }
}

void handle_signal(int signal) {
    stop_signal = 1;
}

// returns socket fd; -1 on error.
int setup_socket(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        syslog(LOG_ERR, "socket: %s", STRERROR);
        return -1;
    }
    // silly
    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "setsockopt: %s", STRERROR);
        close(socket_fd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        syslog(LOG_ERR, "bind: %s", STRERROR)
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, SOMAXCONN) == -1) {
        syslog(LOG_ERR, "listen: %s", STRERROR)
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

void send_from_file(const char* file_path, int send_fd) {

}

void recv_to_file(const char* file_path, int recv_fd) {

}