#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int PORT = 9375;

int main(int argc, char* argv[])
{
    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("!!! Failed to create Socket in admin!!\n");
        exit(-1);
    }
    else
    {
        printf("Socket successfully created in admin, socket fd: %d!\n", sockfd);
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
    {
        printf("!!! Failed to connect to the server from admin!!!\n");
        exit(0);
    }
    else
    {
        printf("Successfully connected to the server from admin with socket fd: %d!\n", sockfd);
    }

    char buff[128];
    ssize_t bread;

    // pthread_t receive_thread;

    // pthread_create(&receive_thread, NULL, &receive, NULL);

    while(1)
    {
        fgets(buff, 128, stdin);
        printf("Buff = %s\n", buff);
        write(sockfd, buff, sizeof(buff));
        if (buff[0] == '0' || buff[0] == '1')
        {
            break;
        }
        bzero(buff, 128);
    }
}

void* receive(void*p)
{

}
