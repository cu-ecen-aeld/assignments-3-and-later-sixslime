#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void handle_signal(int signal);

int setup_socket_listener(int port);

void recv_send_file(const char *file_path, int socket_fd);

static int write_all(int fd, const void *buf, size_t len);

static int send_all(int sockfd, const void *buf, size_t len);

static int send_file_back(const char *file_path, int socket_fd);

