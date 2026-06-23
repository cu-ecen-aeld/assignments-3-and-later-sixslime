#include "aesdsocket.h"

static volatile sig_atomic_t stop_signal = 0;


#define WRITE_PATH "/var/tmp/aesdsocketdata"
#define PORT 9000

int main(int argc, char *argv[]) {
    openlog(argv[0], NULL, LOG_USER);

    // signal link:
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT,  &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "sigaction: %s", strerror(errno));
        closelog(); return -1;
    }
}

void handle_signal(int signal) {
    stop_signal = 1;
}

int setup_listener(int port) {

}

void send_from_file(const char* file_path, int send_fd) {

}

void recv_to_file(const char* file_path, int recv_fd) {
    
}