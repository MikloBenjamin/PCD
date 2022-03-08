#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

typedef struct Request{
    char *message;
} Request;

void* task_server(void* param);

void* run_server(void* port);
