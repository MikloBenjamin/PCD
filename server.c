#include "server.h"

#define MAX_REQUESTS 100
#define MAX_SIZE 1024

Request requests[MAX_REQUESTS];
int sizeof_requests = 0;

void create_request(char* message)
{
    if (sizeof_requests >= MAX_REQUESTS)
    {
        fprintf(stderr, "Request overflow, cannot create new request!\n");
        return;
    }
    Request new_request;
    new_request.message = (char*) malloc(sizeof(char) * strlen(message) + 1);
    strcpy(new_request.message, message);
    requests[sizeof_requests++] = new_request;
    fprintf(stdout, "New request created, size of requests: %d\n", sizeof_requests);
}

void* task_server(void* param)
{
    while (true)
    {
        if (sizeof_requests > 0)
        {
            fprintf(stdout, "%s\n", requests[0].message);
            free(requests[0].message);
            for (int i = 0; i < sizeof_requests; i++)
            {
                requests[i] = requests[i + 1];
            }
            sizeof_requests--;
        }
    }
}

void read_client(int connfd)
{
    char buff[MAX_SIZE];
    int n;

    while(true)
    {
        // bzero(buff, MAX_SIZE);
        // strcpy(buff, "Message received, please be patient.\n");
   
        // // send "message received" back to client
        // write(connfd, buff, sizeof(buff));

        bzero(buff, MAX_SIZE);
   
        read(connfd, buff, sizeof(buff));

        // print buffer which contains the client contents
        // printf("From client: %s\t To client : ", buff);
        create_request(buff);

        bzero(buff, MAX_SIZE);
        strcpy(buff, "Message received, please be patient.\n");
   
        // send "message received" back to client
        write(connfd, buff, sizeof(buff));
   
        // if msg contains "Exit" then server exit and chat ended.
        if (strncmp("exit", buff, 4) == 0) {
            printf("Server Exit...\n");
            break;
        }
    }
}

void* run_server(void* port)
{
    int *port_ref = (int*)(port);
    int PORT = *port_ref;

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("!!! Failed to create Socket !!!\n");
        exit(-1);
    }
    else
    {
        printf("Socket successfully created!\n");
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if ((bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("!!! Failed to bind Socket !!!\n");
        exit(0);
    }
    else
    {
        printf("Succesfully bound socket!\n");
    }

    pthread_t server;
    pthread_create(&server, NULL, task_server, NULL);

    if ((listen(sockfd, 10)) != 0) {
        printf("!!! Server Failed to start listening !!!\n");
        exit(0);
    }
    else
    {
        printf("Server is listening on port: %d!\n", PORT);
    }

    len = sizeof(cli);

    connfd = accept(sockfd, (struct sockaddr*)&cli, &len);
    if (connfd < 0) {
        printf("!!! Server could not accept the client !!!\n");
        exit(0);
    }
    else
    {
        printf("Client connected succesfully!\n");
    }

    read_client(connfd);

    close(sockfd);

    pthread_exit(NULL);
}
