// STUDENT:
// yknow the 1 thread per connection was pretty cool and fun,
// then I tried for an indepedent and accurate 10 second timer and everything went to shit.
// I refuse to use a naive sleep(10) loop where the loop body locks on a mutex.
// I used to think Rust was great, but now I *know* Rust is great if this is what it's replacing.
// Boy do I love signal handling!

#include "aesdsocket.h"

static atomic_int stop_signal = 0;
static pthread_mutex_t write_mutex_global = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
    int listen_fd;
    int init_r = init_program(argc, argv, &listen_fd);
    if (init_r != 0) {
        return init_r;
    }
    
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

    // start timer:
    struct itemerval timer = {
        .it_value = {
            .tv_sec = TIMER_INTERVAL_SECONDS,
            .tv_usec = 0,
        },
        .it_interval = {
            .tv_sec = TIMER_INTERVAL_SECONDS,
            .tv_usec = 0,
        },
    };
    if (setitimer(ITIMER_REAL, &timer, NULL) != 0) {
        syslog(LOG_ERR, "setitimer: %s", STRERROR);
        return -1;
    }
    
    return 0;
}

struct signal_listener_args {
    pthread_mutex_t* write_mutex;
};
static void* signal_listener_worker(void* arg) {
    struct signal_listener_args* args = (struct signal_listener_args*)arg;
}
int signal_listener_on_thread(pthread_t* pthread_id, pthread_mutex_t* write_mutex) {

}

struct thread_entry {
    pthread_t value;
    SLIST_ENTRY(thread_entry) entries;
};
struct connection_listener_args {
    pthread_mutex_t* write_mutex;
    int listen_fd;
};
static void* connection_listener_worker(void* arg) {
    struct connection_listener_args* args = (struct connection_listener_args*)arg;
}
int connection_listener_on_thread(int listen_fd, pthread_t* pthread_id, pthread_mutex_t* write_mutex) {

}

struct accept_connection_args {
    int client_fd;
    pthread_mutex_t* write_mutex;
    // this is *shitty*.
    char* ip_str;
};

static void* accept_connection_worker(void* arg) {
    struct accept_connection_args* args = (struct accept_connection_args*)arg;
    recv_send_file(WRITE_PATH, args->client_fd, args->write_mutex);
    close(args->client_fd);
    syslog(LOG_INFO, "Closed connection from %s", args->ip_str);

    free(args->ip_str);
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
    args->write_mutex = write_mutex;
    args->client_fd = client_fd;
    // what are we doing !??
    args->ip_str = malloc(strlen(ip_str) + 1);
    if (args->ip_str == NULL) {
        syslog(LOG_ERR, "malloc ip_str: %s", STRERROR);
        return 1;
    }
    strcpy(args->ip_str, ip_str);

    // create thread:
    if (pthread_create(pthread_id, NULL, accept_connection_worker, args) != 0) {
        syslog(LOG_ERR, "pthread_create: %s", STRERROR);
        free(args);
        return 1;
    }

    return 0;
}

int exit_program(int listen_fd) {
    syslog(LOG_INFO, "Caught signal, exiting");
    close(listen_fd);
    unlink(WRITE_PATH);
    return 0;
}

void on_sigalrm(pthread_mutex_t* write_mutex) {

}

void on_sigint() {
    atomic_store(&stop_signal, 1);
}