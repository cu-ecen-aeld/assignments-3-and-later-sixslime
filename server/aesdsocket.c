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
    int r = init_program(argc, argv, &listen_fd);
    if (r != 0) {
        return r;
    }
    pthread_t signal_listener_thread, connection_listener_thread;
    if (signal_listener_on_thread(&signal_listener_thread, &write_mutex_global) != 0) {
        goto exit;
    }
    if (connection_listener_on_thread(&connection_listener_thread, &write_mutex_global, listen_fd, POLL_MS) != 0) {
        goto exit;
    }

    if (pthread_join(signal_listener_thread, NULL) != 0) {
        syslog(LOG_ERR, "pthread_join: %s", STRERROR);
    }
    if (pthread_join(connection_listener_thread, NULL) != 0) {
        syslog(LOG_ERR, "pthread_join: %s", STRERROR);
    }
    exit:
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

    // setup signal blocking:
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGALRM);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    if ((errno = pthread_sigmask(SIG_BLOCK, &blocked, NULL)) != 0) {
        syslog(LOG_ERR, "pthread_sigmask: %s", STRERROR);
        return -1;
    }

    // start timer:
    struct itimerval timer = {
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
int exit_program(int listen_fd) {
    struct itimerval timeroff = {0};
    setitimer(ITIMER_REAL, &timeroff, NULL);
    syslog(LOG_INFO, "Caught signal, exiting");
    close(listen_fd);
    unlink(WRITE_PATH);
    return 0;
}

struct signal_listener_args {
    pthread_mutex_t* write_mutex;
};
static void* signal_listener_worker(void* arg) {
    struct signal_listener_args* args = (struct signal_listener_args*)arg;

    sigset_t listen_set;
    sigemptyset(&listen_set);
    sigaddset(&listen_set, SIGALRM);
    sigaddset(&listen_set, SIGINT);
    sigaddset(&listen_set, SIGTERM);

    while (atomic_load(&stop_signal) == 0) {
        int sig;
        int wait_r = sigwait(&listen_set, &sig);
        if (wait_r != 0) {
            if (wait_r == EINTR) continue;
            syslog(LOG_ERR, "sigwait: %s", STRERROR);
            break;
        }
        if (sig == SIGALRM) {
            on_sigalrm(args->write_mutex);
            continue;
        }
        // SIGINT or SIGTERM:
        on_sigint();
    }

    free(args);
    return NULL;
}
int signal_listener_on_thread(pthread_t* pthread_id, pthread_mutex_t* write_mutex) {
    // worker args:
    struct signal_listener_args* args = malloc(sizeof(*args));
    if (args == NULL) {
        syslog(LOG_ERR, "malloc signal_listener_args: %s", STRERROR);
        return 1;
    }
    args->write_mutex = write_mutex;
    // create worker:
    if (pthread_create(pthread_id, NULL, signal_listener_worker, args) != 0) {
        syslog(LOG_ERR, "pthread_create: %s", STRERROR);
        free(args);
        return 1;
    }
    return 0;
}

struct thread_entry {
    pthread_t value;
    SLIST_ENTRY(thread_entry) entries;
};
struct connection_listener_args {
    pthread_mutex_t* write_mutex;
    int listen_fd;
    int poll_ms;
};
static void* connection_listener_worker(void* arg) {
    struct connection_listener_args* args = (struct connection_listener_args*)arg;

    // init slist:
    SLIST_HEAD(thread_slist, thread_entry);
    struct thread_slist slist_head;
    SLIST_INIT(&slist_head);

    struct pollfd poll_handle = { .fd = args->listen_fd, .events = POLLIN };

    while (atomic_load(&stop_signal) == 0) {
        // poll:
        int poll_r = poll(&poll_handle, 1, args->poll_ms);
        if (poll_r == -1) {
            syslog(LOG_ERR, "poll: %s", STRERROR);
            continue;
        }
        if (poll_r == 0 || !(poll_handle.revents & POLLIN)) { continue; }
        // create connection thread:
        pthread_t connection_pthread;
        if (accept_connection_on_thread(&connection_pthread, args->write_mutex, args->listen_fd) != 0) {
            syslog(LOG_WARNING, "connection not accepted.");
            continue;
        }
        // add thread to list:
        struct thread_entry* entry = malloc(sizeof(*entry));
        entry->value = connection_pthread;
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
    free(args);
    return NULL;
}
int connection_listener_on_thread(pthread_t* pthread_id, pthread_mutex_t* write_mutex, int listen_fd, int poll_ms) {
    // worker args:
    struct connection_listener_args* args = malloc(sizeof(*args));
    if (args == NULL) {
        syslog(LOG_ERR, "malloc connection_listener_args: %s", STRERROR);
        return 1;
    }
    args->listen_fd = listen_fd;
    args->write_mutex = write_mutex;
    args->poll_ms = poll_ms;
    // create worker:
    if (pthread_create(pthread_id, NULL, connection_listener_worker, args) != 0) {
        syslog(LOG_ERR, "pthread_create: %s", STRERROR);
        free(args);
        return 1;
    }
    return 0;
}

struct accept_connection_args {
    pthread_mutex_t* write_mutex;
    int listen_fd;
};
static void* accept_connection_worker(void* arg) {
    struct accept_connection_args* args = (struct accept_connection_args*)arg;
    // accept:
    struct sockaddr_in client_addr;
    socklen_t csocket_len = sizeof(client_addr);
    int client_fd = accept(args->listen_fd, (struct sockaddr *)&client_addr, &csocket_len);
    if (client_fd == -1) {
        if (errno == EINTR) goto exit;
        syslog(LOG_ERR, "accept: %s", STRERROR);
        goto exit;
    }
    // log readable ip:
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
    syslog(LOG_INFO, "Accepted connection from %s", ip_str);
    
    recv_send_file(WRITE_PATH, client_fd, args->write_mutex);
    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", ip_str);

    exit:
    free(args);
    return NULL;
}
int accept_connection_on_thread(pthread_t* pthread_id, pthread_mutex_t* write_mutex, int listen_fd) {

    // worker args:
    struct accept_connection_args* args = malloc(sizeof(*args));
    if (args == NULL) {
        syslog(LOG_ERR, "malloc accept_connection_args: %s", STRERROR);
        return 1;
    }
    args->write_mutex = write_mutex;
    args->listen_fd = listen_fd;
    // create worker:
    if (pthread_create(pthread_id, NULL, accept_connection_worker, args) != 0) {
        syslog(LOG_ERR, "pthread_create: %s", STRERROR);
        free(args);
        return 1;
    }
    return 0;
}

void on_sigalrm(pthread_mutex_t* write_mutex) {
    syslog(LOG_INFO, "Handling sigalrm");
    char buffer[128];
    // acquire lock before getting time probably good no?
    pthread_mutex_lock(write_mutex);
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    size_t str_len = strftime(buffer, sizeof(buffer), "timestamp:%a, %d, %b, %Y, %H:%M:%S %z\n", &tm_info);
    if (str_len == 0) {
        syslog(LOG_ERR, "strftime failed");
        pthread_mutex_unlock(write_mutex);
        return;
    }
    syslog(LOG_INFO, "TIMER: %s", buffer);
    // write:
    int write_fd = open(WRITE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (write_fd == -1) {
        syslog(LOG_ERR, "open %s: %s", WRITE_PATH, STRERROR);
        pthread_mutex_unlock(write_mutex);
        return;
    }
    if (write_all(write_fd, &buffer, str_len) < 0) {
        syslog(LOG_ERR, "write: %s", STRERROR);
        close(write_fd);
        pthread_mutex_unlock(write_mutex);
        return;
    }
    close(write_fd);
    pthread_mutex_unlock(write_mutex);
}

void on_sigint() {
    syslog(LOG_INFO, "Handling sigint");
    atomic_store(&stop_signal, 1);
}