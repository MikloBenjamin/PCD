#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "admin.h"
#include "server.h"

#define PORT 7366

void* run_admin(void* port);
void* run_server(void* port);

int main(int argc, char* argv[])
{
    pthread_t server, admin;
  
    int port = PORT;

    pthread_create(&server, NULL, run_server, &port);
    // pthread_create(&admin, NULL, run_admin, &port);

    pthread_join(server, NULL);
    // pthread_join(admin, NULL);

    pthread_exit(NULL);
}
