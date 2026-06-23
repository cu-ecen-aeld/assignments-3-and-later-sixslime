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

void send_from_file(const char* file_path, int send_fd);

void recv_to_file(const char* file_path, int recv_fd);