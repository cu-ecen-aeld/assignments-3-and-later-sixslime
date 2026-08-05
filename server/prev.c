#include "prev.h"

void recv_send_file(const char *file_path, int socket_fd, pthread_mutex_t* write_mutex)
{
    char *line = NULL;
    size_t line_len = 0;
    size_t line_cap = 0;
    int lock_held = 0;
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
                // lock:
                if (USE_AESD_CHAR_DEVICE != 1) {
                    pthread_mutex_lock(write_mutex);
                    lock_held = 1;
                }
                // open file:
                int dev_fd = open(file_path, O_RDWR | O_CREAT | O_APPEND, 0644);
                if (dev_fd == -1) {
                    syslog(LOG_ERR, "open %s: %s", file_path, STRERROR);
                    goto out;
                }
                // check for AESDCHAR_IOCSEEKTO command:
                struct aesd_seekto seekto;
                if (sscanf(line, "AESDCHAR_IOCSEEKTO:%u,%u\n", &seekto.write_cmd, &seekto.write_cmd_offset) == 2) {
                    int ioctl_r = ioctl(dev_fd, AESDCHAR_IOCSEEKTO, &seekto);
                    if (ioctl_r != 0) {
                        syslog(LOG_ERR, "ioctl seekto failed: %d", ioctl_r);
                    }
                }
                // write to file otherwise:
                else {
                    if (write_all(dev_fd, line, line_len) < 0) {
                        syslog(LOG_ERR, "write: %s", STRERROR);
                        close(dev_fd);
                        goto out;
                    }
                }
                // send file back to socket:
                if (send_file_back(dev_fd, socket_fd) < 0) {
                    syslog(LOG_ERR, "send_file_back: %s", STRERROR);
                    close(dev_fd);
                    goto out;
                }
                // last unlock:
                if (USE_AESD_CHAR_DEVICE != 1) {
                    lock_held = 0;
                    pthread_mutex_unlock(write_mutex);
                }
                close(dev_fd);
                line_len = 0;
            }
        }
    }

    out:
    if (lock_held == 1) {
        pthread_mutex_unlock(write_mutex);
    }
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

int send_file_back(int read_fd, int socket_fd)
{
    char buffer[8192];
    for (;;) {
        ssize_t n_read = read(read_fd, buffer, sizeof buffer);
        if (n_read < 0) {
            syslog(LOG_ERR, "read %s: %s", file_path, STRERROR);
            return -1;
        }
        if (n_read == 0) break;

        if (send_all(socket_fd, buffer, (size_t)n_read) < 0) {
            return -1;
        }
    }

    close(read_fd);
    return 0;
}

// returns socket fd; -1 on error.
int setup_socket_listener(int port) 
{
    int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
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

// C is disguisting
int daemonize(void)
{
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork: %s", STRERROR);
        return -1;
    }

    if (pid > 0) {
        _exit(0);
    }

    if (setsid() < 0) {
        syslog(LOG_ERR, "setsid: %s", STRERROR);
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "fork: %s", STRERROR);
        return -1;
    }

    if (pid > 0) {
        _exit(0);
    }

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "chdir(\"/\"): %s", STRERROR);
        return -1;
    }

    umask(0);

    int null_fd = open("/dev/null", O_RDWR);
    if (null_fd < 0) {
        syslog(LOG_ERR, "open(\"/dev/null\"): %s", STRERROR);
        return -1;
    }

    if (dup2(null_fd, STDIN_FILENO) < 0 ||
        dup2(null_fd, STDOUT_FILENO) < 0 ||
        dup2(null_fd, STDERR_FILENO) < 0) {
        syslog(LOG_ERR, "dup2: %s", STRERROR);
        close(null_fd);
        return -1;
    }

    if (null_fd > STDERR_FILENO) {
        close(null_fd);
    }
    return 0;
}