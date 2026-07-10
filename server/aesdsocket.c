
#include "aesdsocket.h"

static volatile sig_atomic_t stop_signal = 0;

struct thread_entry {
    thread_t thread;
    SLIST_ENTRY(thread_entry) entries;
};

int main(int argc, char *argv[])
{
    int listen_fd;
    int init_r = init_program(argc, argv, &listen_fd);
    if (init_r != 0) {
        return init_r;
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

    return exit_program(listen_fd);
}

// set stop_signal to 1, let operations finish.
void handle_signal(int signal) 
{
    (void)signal;
    stop_signal = 1;
}

int init_program(int argc, char *argv[], int* listen_fd) {
    openlog(NULL, LOG_PID | LOG_CONS, LOG_USER);

    // try setup listener:
    *listen_fd = setup_socket_listener(LISTEN_PORT);
    if (*listen_fd == -1) {
        return -1;
    }

    // daemonize if flag found:
    int opt;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                if (daemonize() == -1) {
                    close(*listen_fd);
                    return -1;
                }
                break;
            default:
                abort();
                break;
        }
    }

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
    return 0;
}

int exit_program(int listen_fd) {
    syslog(LOG_INFO, "Caught signal, exiting");
    close(listen_fd);
    unlink(WRITE_PATH);
    return 0;
}