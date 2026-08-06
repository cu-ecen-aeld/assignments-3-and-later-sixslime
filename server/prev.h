#include "std_includes.h"

void recv_send_file(const char *file_path, int socket_fd, pthread_mutex_t* write_mutex);

int write_all(int fd, const void *buf, size_t len);

int send_all(int sockfd, const void *buf, size_t len);

int send_file_back(int file_fd, int socket_fd);

int daemonize(void);

int setup_socket_listener(int port);