#include <stdio.h>
#include <pthread.h>
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
#include <sys/queue.h>
#include <sys/time.h>
#include <time.h>
#include <stdatomic.h>
#include <poll.h>

#ifndef USE_AESD_CHAR_DEVICE
    #define USE_AESD_CHAR_DEVICE 1
#endif

#define STRERROR strerror(errno)
#define LISTEN_PORT 9000
#define TIMER_INTERVAL_SECONDS 10
#define POLL_MS 5

#if USE_AESD_CHAR_DEVICE == 1
    #define WRITE_PATH "/dev/aesdchar"
#else
    #define WRITE_PATH "/var/tmp/aesdsocketdata"
#endif