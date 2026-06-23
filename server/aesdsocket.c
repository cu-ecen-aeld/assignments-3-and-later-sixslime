// I used google and AI here pretty liberally. I'm a C# dev not a C dev.
// It would be impractical to document every use, but I primarly would ask AI for things like "write a function that does X" or "how do I do Y, provide an example" and then write *my own* code, using the response/example as reference.
// I did not just copy-and-paste.
#include "aesdsocket.h"

#define WRITE_PATH "/var/tmp/aesdsocketdata"
#define LISTEN_PORT 9000
#define STRERROR strerror(errno)

static volatile sig_atomic_t stop_signal = 0;

int main(int argc, char *argv[]) {
    openlog(NULL, LOG_PID | LOG_CONS, LOG_USER);

    // signal handling:
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT,  &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "sigaction: %s", STRERROR);
        return -1;
    }

    // setup listener:
    int listen_fd = setup_socket_listener(LISTEN_PORT);
    if (listen_fd == -1) {
        return -1;
    }

    // loop until signal recieved:
    while (stop_signal == 0) {

        // accept connection:
        struct sockaddr_in client_addr;
        socklen_t csocket_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &csocket_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "accept: %s", STRERROR);
            continue;
        }
        
        // make readable ip:
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        syslog(LOG_INFO, "Accepted connection from %s", ip_str);

        // main recieve and send:
        recv_send_file(WRITE_PATH, client_fd);

        // close:
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", ip_str);
    }

    // exit:
    syslog(LOG_INFO, "Caught signal, exiting");
    close(listen_fd);
    unlink(WRITE_PATH);
    return 0;
}

// set stop_signal to 1, let operations finish.
void handle_signal(int signal) {
    (void)signal;
    stop_signal = 1;
}

// returns socket fd; -1 on error.
int setup_socket_listener(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        syslog(LOG_ERR, "socket: %s", STRERROR);
        return -1;
    }
    int opt = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
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
        syslog(LOG_ERR, "bind: %s", STRERROR);
        close(socket_fd);
        return -1;
    }

    if (listen(socket_fd, SOMAXCONN) == -1) {
        syslog(LOG_ERR, "listen: %s", STRERROR);
        close(socket_fd);
        return -1;
    }

    return socket_fd;
}

void recv_send_file(const char *file_path, int socket_fd)
{
    char *line = NULL;
    size_t line_len = 0;
    size_t line_cap = 0;

    char buffer[4096];

    for (;;) {
        ssize_t n_received = recv(socket_fd, buffer, sizeof buffer, 0);
        if (n_received < 0) {
            syslog(LOG_ERR, "recv: %s", STRERROR);
            break;
        }
        if (n_received == 0) {
            break;
        }

        for (ssize_t i = 0; i < n_received; i++) {
            char c = buffer[i];

            if (line_len + 1 > line_cap) {
                size_t new_cap = (line_cap == 0) ? 1024 : line_cap * 2;
                while (new_cap < line_len + 1) new_cap *= 2;

                char *tmp = realloc(line, new_cap);
                if (!tmp) {
                    syslog(LOG_ERR, "realloc: %s", STRERROR);
                    goto out;
                }

                line = tmp;
                line_cap = new_cap;
            }

            line[line_len++] = c;

            if (c == '\n') {
                int write_fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (write_fd == -1) {
                    syslog(LOG_ERR, "open %s: %s", file_path, STRERROR);
                    goto out;
                }

                if (write_all(write_fd, line, line_len) < 0) {
                    syslog(LOG_ERR, "write: %s", STRERROR);
                    close(write_fd);
                    goto out;
                }

                close(write_fd);

                if (send_file_back(file_path, socket_fd) < 0) {
                    syslog(LOG_ERR, "send_file_back: %s", STRERROR);
                    goto out;
                }

                line_len = 0;
            }
        }
    }

out:
    free(line);
}

int write_all(int write_fd, const void *buffer, size_t len)
{
    const char *pbuf = (const char *)buffer;
    size_t off = 0;

    while (off < len) {
        ssize_t n_written = write(write_fd, pbuf + off, len - off);
        if (n_written < 0) {
            syslog(LOG_ERR, "write: %s", STRERROR);
            return -1;
        }
        off += (size_t)n_written;
    }
    return 0;
}

int send_all(int socket_fd, const void *buffer, size_t len)
{
    const char *pbuf = (const char *)buffer;
    size_t off = 0;

    while (off < len) {
        ssize_t n_sent = send(socket_fd, pbuf + off, len - off, 0);
        if (n_sent < 0) {
            syslog(LOG_ERR, "send: %s", STRERROR);
            return -1;
        }
        off += (size_t)n_sent;
    }

    return 0;
}

int send_file_back(const char *file_path, int socket_fd)
{
    int read_fd = open(file_path, O_RDONLY);
    if (read_fd == -1) {
        syslog(LOG_ERR, "open %s: %s", file_path, STRERROR);
        return -1;
    }

    char buffer[8192];
    for (;;) {
        ssize_t n_read = read(read_fd, buffer, sizeof buffer);
        if (n_read < 0) {
            syslog(LOG_ERR, "read %s: %s", file_path, STRERROR);
            close(read_fd);
            return -1;
        }
        if (n_read == 0) break;

        if (send_all(socket_fd, buffer, (size_t)n_read) < 0) {
            close(read_fd);
            return -1;
        }
    }

    close(read_fd);
    return 0;
}