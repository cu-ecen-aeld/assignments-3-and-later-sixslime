// I used google and AI here pretty liberally. I'm a C# dev not a C dev.
// It would be impractical to document every use, but I primarly would ask AI for things like "write a function that does X" or "how do I do Y, provide an example" and then write *my own* code, using the response/example as reference.
// I did not just copy-and-paste.

#include "aesdsocket.h"

#define WRITE_PATH "/var/tmp/aesdsocketdata"
#define LISTEN_PORT 9000
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
        closelog();
        return -1;
    }

    int listen_fd = setup_socket_listener(LISTEN_PORT);
    if (listen_fd == -1) {
        return -1;
    }

    while (!stop_signal) {
        struct sockaddr_in client_addr;
        socklen_t csocket_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &csocket_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "accept: %s", STRERROR);
            continue;
        }
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        syslog(LOG_INFO, "Accepted connection from %s", ip_str);

        recv_to_file(client_fd, WRITE_PATH);
        send_from_file(client_fd, WRITE_PATH);

        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", ip_str);
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    close(listen_fd);
    unlink(WRITE_PATH);
    return 0;
}

void handle_signal(int signal) {
    stop_signal = 1;
}

// returns socket fd; -1 on error.
int setup_socket_listener(int port) {
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

void recv_to_file(const char* file_path, int recv_fd) {
    int read_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (read_fd == -1) {
        syslog(LOG_ERR, "opening %s: %s", file_path, STRERROR);
        return;
    }
    char buffer[4096];
    char buf[4096];
    ssize_t n;
    for (;;) {
        n = recv(connfd, buf, sizeof(buf), 0);
        if (n == 0) break;
        if (n == -1) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "recv: %s", STRERROR);
            break;
        }
        // surely partial writes wont happen.
        if (write(fd, buf, n) == -1) {
            syslog(LOG_ERR, "write %s: %s", path, STRERROR);
        }
    }
    close(read_fd);
}

void send_from_file(const char* file_path, int send_fd) {

}

