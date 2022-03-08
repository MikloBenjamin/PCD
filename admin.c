#include "admin.h"

#define MAX_REQUESTS 10

void* task_admin(void* param)
{
    int *param_ref = (int*)(param);
    int param_value = *param_ref;
    sleep(param_value);
    fprintf(stderr, "Hello from ADMIN task: %d\n", param_value);
}

void* run_admin(void* port)
{
    pthread_t requests[MAX_REQUESTS];
    for (int i = 0; i < MAX_REQUESTS; i++)
    {
        pthread_create(&requests[i], NULL, task_admin, &i);
    }
    pthread_exit(NULL);
}
