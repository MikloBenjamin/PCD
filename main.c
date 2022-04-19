#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "server.h"

#define PORT 7366

void* run_server(void* port);

int main(int argc, char* argv[])
{
    pthread_t server;
  
    int port = PORT;

    pthread_create(&server, NULL, run_server, &port);

    pthread_join(server, NULL);

    return EXIT_SUCCESS;
}
