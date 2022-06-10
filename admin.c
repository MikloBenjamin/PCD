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

typedef enum OptionType {
    NEGATIV = 0,
    SEPIA = 1,
    BLUR = 2,
    BLACK_AND_WHITE = 3,
    DEFAULT = 4
} Option;

int PORT = 9375;

void showRead();
void* receive(void* param);
void convertUptimeToHMS(int uptime, int *hours, int *minutes, int *seconds);

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

    pthread_t receive_thread;

    pthread_create(&receive_thread, NULL, &receive, &sockfd);
    showRead();
    while(1)
    {
        fgets(buff, 128, stdin);
        write(sockfd, buff, sizeof(buff));
        if (buff[0] == '0' || buff[0] == '1')
        {
            break;
        }

        if (buff[0] == '3' || buff[0] == '4')
        {
            showRead();
        }
        bzero(buff, 128);
    }
}

void showRead()
{
    int choice = -1;
    printf("\n\nCommands:\n");
    printf("\tDISCONNECT               <=> 0\n");
    printf("\tEND                      <=> 1\n");
    printf("\tINFO                     <=> 2\n");
    printf("\tKILL_CLIENT BY THREAD_ID <=> 3 CLIENT_THREAD_ID\n\n");
    printf("\tKILL_CLIENT BY SOCKET_ID <=> 4 CLIENT_SOCKET_ID\n\n");
    printf("Command = ");
    fflush(stdout);
}

void convertUptimeToHMS(int uptime, int *hours, int *minutes, int *seconds)
{
	*hours   = (uptime/3600); 
	*minutes = (uptime - (3600 * (*hours))) / 60;
	*seconds = (uptime - (3600 * (*hours)) - ((*minutes) * 60));
}

char* getOptionAsString(Option option)
{
    switch(option)
    {
        case(NEGATIV):
            return "NEGATIV";
        case(SEPIA):
            return "SEPIA";
        case(BLUR):
            return "BLUR";
        case(BLACK_AND_WHITE):
            return "BLACK&WHITE";
    }
    return "";
}

void* receive(void* param)
{
    fprintf(stderr, "Admin receive started\n");
    int sockfd = *(int*)param;
    char buffer[1024];
    ssize_t bytes_read;

    while (1)
    {
        bytes_read = recv(sockfd, buffer, 1024, 0);
        system("clear && printf '\e[3J'");
        if (bytes_read == 0)
        {
            fprintf(stderr, "Admin receive END, bytes read = 0\n");
            break;
        }
        fprintf(stderr, "-------------------------------------------------------- \n\n");
        fprintf(stderr, "-------------- Admin receive started ------------\n");

        int nr_of_clients = 0xff & buffer[0];
        nr_of_clients |= (0xff & buffer[1]) << 8;

        int uptime = 0xff & buffer[2];
        uptime |= (0xff & buffer[3]) << 8;

        fprintf(stderr, "\n\n---------------- SERVER INFO ----------------\n\n");
        fprintf(stderr, "Number of clients: %d\n\n", nr_of_clients);
        int hours, minutes, seconds;
        convertUptimeToHMS(uptime, &hours, &minutes, &seconds);

        fprintf(stderr, "Uptime: %dh : %dm : %ds\n\n", hours, minutes, seconds);

        bzero(buffer, 1024);
        while (nr_of_clients > 0)
        {
            fprintf(stderr, "\n\t ***** CLIENT INFO ***** \n");
            bytes_read = recv(sockfd, buffer, 1024, 0);

            char* number = strtok(buffer, "|");
            fprintf(stderr, "\t\tSocket ID  : %s\n", number);

            number = strtok(NULL, "|");
            fprintf(stderr, "\t\tThread ID  : %s\n", number);

            number = strtok(NULL, "|");
            uptime = atoi(number);
            convertUptimeToHMS(uptime, &hours, &minutes, &seconds);
    
            fprintf(stderr, "\t\tUptime     : %dh : %dm : %ds\n", hours, minutes, seconds);

            number = strtok(NULL, "|");
            fprintf(stderr, "\t\tNr. Imgs.  : %s\n", number);

            number = strtok(NULL, "|");
            int option = atoi(number);
            fprintf(stderr, "\t\tOption     : %s\n", getOptionAsString(option));

            bzero(buffer, 1024);
            nr_of_clients--;
        }
        showRead();
    }
}
