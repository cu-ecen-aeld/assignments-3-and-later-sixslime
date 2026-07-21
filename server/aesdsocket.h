#include "std_includes.h"
#include "prev.h"

int init_program(int argc, char* argv[], int* listen_fd);

int exit_program(int listen_fd);

int accept_connection_on_thread(int listen_fd, pthread_t* pthread_id, pthread_mutex_t* write_mutex);

int signal_listener_on_thread(pthread_t* pthread_id, pthread_mutex_t* write_mutex);

int connection_listener_on_thread(int listen_fd, pthread_t* pthread_id, pthread_mutex_t* write_mutex);

void on_sigalrm(pthread_mutex_t* write_mutex);

void on_sigint();