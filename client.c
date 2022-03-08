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
#include <arpa/inet.h> 

#include "common.h"

#define MAX_SIZE 1024

bool message_received = false;
bool server_closed = false;

pthread_t receiver, messenger;

void* receive_message(void *socketfd)
{
    int *socket_ref = (int*)(socketfd);
    int sockfd = *socket_ref;
    char buff[MAX_SIZE];
    int n;
    while (true)
    {
        bzero(buff, sizeof(buff));
        
        n = 0;
        read(sockfd, buff, sizeof(buff));
        message_received = true;

        printf("\nMessage received from server: %s and size: %ld\n", buff, strlen(buff));
        message_received = false;
        if ((strncmp(buff, "exit", 4)) == 0 || strlen(buff) == 0)
        {
            printf("\n!!! SERVER CLOSED CONNECTION !!!\n");
            printf("Client Exit!\n");
            pthread_kill(messenger, 9);
            break;
        }
    }
}

void* create_message(void *socketfd)
{
    int *socket_ref = (int*)(socketfd);
    int sockfd = *socket_ref;
    char buff[MAX_SIZE];
    int n;

    printf("Enter the message you want to send: ");
    while(true)
    {
        bzero(buff, sizeof(buff));
        
        n = 0;

        while (buff[n - 1] != '\n')
        {
            if (message_received)
            {
                printf("\nEnter the message you want to send: %s", buff);
                message_received = false;
            }
            buff[n++] = getchar();
        }

        strcpy(buff, trim(buff));

        if (strcmp(buff, "\n") != 0)
        {
            printf("Sending message: %s\n", buff);
            write(sockfd, buff, sizeof(buff));
            bzero(buff, sizeof(buff));
            printf("Enter the message you want to send: ");
        }
    }
}

int main(int argc, char* argv[])
{
    int PORT = 7366;

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
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("!!! Failed to connect to the server !!!\n");
        exit(0);
    }
    else
    {
        printf("Successfully connected to the server!\n");
    }

    pthread_create(&receiver, NULL, receive_message, &sockfd);
    pthread_create(&messenger, NULL, create_message, &sockfd);

    pthread_exit(NULL);
    close(sockfd);
}

