#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[]) {
    openlog(NULL, 0, LOG_USER);
    if (argc != 3) {
        syslog(LOG_ERR, "Two args are required: <writefile> <writestr>\n");
        return 1;
    }

    char* file_name = argv[1];

    FILE *file_obj = fopen(file_name, "w");

    if (file_obj == NULL) {
        syslog(LOG_ERR, "Error opening file %s: %s\n", file_name, strerror( errno ));
        return 1;
    }

    char* file_content = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s", file_content, file_name);
    fputs(file_content, file_obj);
    fclose(file_obj);
    return 0;
}