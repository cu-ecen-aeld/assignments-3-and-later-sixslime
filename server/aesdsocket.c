#include "aesdsocket.h"

static volatile sig_atomic_t stop_signal = 0;

struct thread_entry {
    pthread_t value;
    SLIST_ENTRY(thread_entry) entries;
};

int main(int argc, char *argv[])
{
    int listen_fd;
    int init_r = init_program(argc, argv, &listen_fd);
    if (init_r != 0) {
        return init_r;
    }
    // mutex:
    pthread_mutex_t write_mutex;

    // init slist:
    SLIST_HEAD(thread_slist, thread_entry);
    struct thread_slist slist_head;
    SLIST_INIT(&slist_head);

    // loop until signal recieved:
    while (stop_signal == 0) {
        // accept on thread:
        pthread_t pthread_id;
        if (accept_connection_on_thread(listen_fd, &pthread_id, &write_mutex) != 0) {
            syslog(LOG_WARNING, "connection not accepted.");
            continue;
        }
        // add thread to list:
        struct thread_entry* entry = malloc(sizeof(*entry));
        entry->value = pthread_id;
        SLIST_INSERT_HEAD(&slist_head, entry, entries);
    }
    // join and free threads:
    struct thread_entry* np;
    while (!SLIST_EMPTY(&slist_head)) {
        np = SLIST_FIRST(&slist_head);
        if (pthread_join(np->value, NULL) != 0) {
            syslog(LOG_ERR, "pthread_join: %s", STRERROR);
        }
        SLIST_REMOVE_HEAD(&slist_head, entries);
        free(np);
    }

    return exit_program(listen_fd);
}

struct worker_args {
    int client_fd;
    pthread_mutex_t* write_mutex;
};

static void* thread_work(void* arg) {
    struct worker_args* args = (struct worker_args*)arg;
    recv_send_file(WRITE_PATH, args->client_fd, args->write_mutex);
    close(args->client_fd);
    syslog(LOG_INFO, "Closed connection from %s", ip_str);

    free(args);
    return NULL;
}

int accept_connection_on_thread(int listen_fd, pthread_t* pthread_id, pthread_mutex_t* write_mutex) {

    // accept:
    struct sockaddr_in client_addr;
    socklen_t csocket_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &csocket_len);
    if (client_fd == -1) {
        if (errno == EINTR) return 1;
        syslog(LOG_ERR, "accept: %s", STRERROR);
        return 1;
    }
    
    // log readable ip:
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    syslog(LOG_INFO, "Accepted connection from %s", ip_str);

    // construct worker args:
    struct worker_args* args = malloc(sizeof(*args));
    if (args == NULL) {
        syslog(LOG_ERR, "malloc worker args: %s", STRERROR);
        return 1;
    }

    // create thread:
    if (pthread_create(pthread_id, NULL, thread_work, args) != 0) {
        syslog(LOG_ERR, "pthread_create: %s", STRERROR);
        free(args);
        return 1;
    }

    return 0;
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

void handle_signal(int signal) 
{
    (void)signal;
    stop_signal = 1;
}