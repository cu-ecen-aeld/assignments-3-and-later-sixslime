#include "std_includes.h"
#include "prev.h"

void handle_sigint(int signal);

void handle_sigalrm(int signal);

int init_program(int argc, char* argv[], int* listen_fd);

int exit_program(int listen_fd);

int accept_connection_on_thread(int listen_fd, pthread_t* pthread_id, pthread_mutex_t* write_mutex);

int timer_on_thread(int interval_seconds, pthread_t* pthread_id, pthread_mutex_t* write_mutex);