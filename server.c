#include "server.h"

#define MAX_REQUESTS 100
#define MAX_SIZE 1024
#define HEADER_SIZE 3
#define MAX_CLIENTS 50
#define ADDR "127.0.0.1"
#define SA struct sockaddr

typedef enum MessageType{
    CONFIRMATION = 1,
    NUMBER_OF_PACKETS = 2,
    PACKET = 3,
    ERROR = 4
} MessageType;

typedef enum AdminMessageType{
    DISCONNECT = 0,
    END = 1
} AdminMessageType;

typedef struct Request
{
    MessageType message_type;       // X - 1st byte from buffer
    unsigned length;        // Y - 2nd and 3rd byte from buffer *MASKED*
    char *message;          // Z - Y number of bytes - MAX: 1021, when X = 3 
} Request;

typedef struct ServerSocket
{
    int socket_id;
    struct sockaddr_in socket_address;
} ServerSocket;

int sizeof_requests = 0;

pthread_t admin, wait_clients, wait_admins;
pthread_t client_threads[MAX_CLIENTS] = {};

sem_t end_server;

int effective_msg_length = MAX_SIZE - HEADER_SIZE;
int image_number = 0;

int client_sockets[MAX_CLIENTS] = {};
int admins = 0, waiting_clients = 1;
struct sigaction signal_handler;

void handle_sigint(int sig_number)
{
    waiting_clients = 0;
    // Sys call to connect random client to end wait client thread
    sem_post(&end_server);
}

void* read_admin(void* conn);
void* wait_admin(void* param);

void* wait_for_clients(void* server);
void* serve_client(void* conn);

void* run_server(void* port)
{
    int PORT = *(int*)port;

    signal(SIGINT, handle_sigint);

    int opt = 1; // 1 TRUE, 0 FALSE
    int sockfd;
    ServerSocket server_socket;

    server_socket.socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket.socket_id == -1) {
        printf("!!! Failed to create Socket !!!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Socket successfully created!\n");
    }

    if( setsockopt(server_socket.socket_id, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 )  
    {  
        perror("setsockopt");  
        exit(EXIT_FAILURE);  
    } 

    bzero(&server_socket.socket_address, sizeof(server_socket.socket_address));

    server_socket.socket_address.sin_family = AF_INET;
    server_socket.socket_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_socket.socket_address.sin_port = htons(PORT);

    if ((bind(server_socket.socket_id, (struct sockaddr*)&server_socket.socket_address, sizeof(server_socket.socket_address))) != 0)
    {
        printf("!!! Failed to bind Socket !!!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Succesfully bound socket!\n");
    }

    if ((listen(server_socket.socket_id, 5)) != 0) {
        printf("!!! Server Failed to start listening !!!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Server is listening on port: %d!\n", PORT);
    }
    sem_init(&end_server, 0, 0);

    pthread_create(&wait_clients, NULL, wait_for_clients, &server_socket);
    pthread_create(&wait_admins, NULL, wait_admin, NULL);

    printf("Waiting for admin to end the server\n");
    sem_wait(&end_server);
    printf("Ending server\n");

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        pthread_join(client_threads[i], NULL);
        if (client_sockets[i] != 0)
        {
            printf("Closing client: %d\n", i);
            close(client_sockets[i]);
        }
    }
    printf("end main\n");
    return EXIT_SUCCESS;
}

void* read_admin(void* conn)
{
    int connfd = *(int*)conn;
    char buff[128];
    ssize_t bread;
    printf("Admin thread started\n");

    while(admins == 1 && (bread = read(connfd, buff, sizeof(buff))) > 0)
    {
        printf("Read from admin: %s , first char:int equivalent %d\n", buff, (int)buff[0]);
        switch ((int)(buff[0] - 48))
        {
            case DISCONNECT:
                admins = 0;
                break;
            case END:
                admins = -1;
                // Sys call to connect random client to end wait client thread
                sem_post(&end_server);
                break;
            default:
                break;
        }
        bzero(buff, sizeof(buff));
    }
    fprintf(stderr, "Socket closed\n");
    close(connfd);
    printf("Admin thread ended\n");
}

void* wait_admin(void* param)
{
    int PORT = 9375;

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
   
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed for admin...\n");
        exit(0);
    }
    else
        printf("Socket successfully created for admin..\n");
    bzero(&servaddr, sizeof(servaddr));
   
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
   
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed for admin...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded for admin..\n");
   
    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("Listen failed for admin...\n");
        exit(0);
    }
    else
        printf("Server listening for admin\n");
    len = sizeof(cli);

   
    while (1)
    {
        if (admins == 0)
        {
            connfd = accept(sockfd, (SA*)&cli, &len);
            if (connfd < 0) {
                printf("server accept admin failed...\n");
                exit(0);
            }
            else
            {
                printf("Admin connected on connection fd: %d\n", connfd);
                admins = 1;
                pthread_create(&admin, NULL, read_admin, &connfd);
            }
        }
        else if(admins == -1)
        {
            break;
        }
    }
    close(sockfd);
}

void* wait_for_clients(void* server)
{
    ServerSocket *server_socket = (ServerSocket*)server;
    fprintf(stderr, "socket_id: %d , socket_address:\n",
            server_socket->socket_id);
    fprintf(stderr, "Starting wait for clients...\n");

    int len = sizeof(server_socket->socket_address);


    while (1){
        if (waiting_clients == 0)
        {
            break;
        }

        fprintf(stderr, "Waiting client...\n");

        int sockfd = accept(
            server_socket->socket_id,
            (struct sockaddr *) &server_socket->socket_address, 
            &len);
        if(sockfd >= 0)
        {
            fprintf(stderr, "client sockfd: %d\n", sockfd);
            int i;
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    client_sockets[i] = sockfd;
                    break;
                }
            }
            if (i != MAX_CLIENTS)
            {
                printf("Client connected\n");
                pthread_create(&client_threads[i], NULL, serve_client, &sockfd);
            }
            // else send Error to client, cant connect
        }
        else
        {
            fprintf(stderr, "Client connect failed, socket < 0: %d\n", sockfd);
        }
    }
    
}

void process(char* image_path){}

void send_back_image(int connfd, char* image_path)
{
    char buffer[MAX_SIZE];

    FILE* image = fopen(image_path, "rb");
    if(!image)
    {
        fprintf(stderr, "RIP!\n");
        exit(1);
    }
    size_t bytes_read;
    int effective_msg_length = MAX_SIZE - HEADER_SIZE;

    fseek(image, 0, SEEK_END);
    long int size = ftell(image);
    fseek(image, 0, SEEK_SET);

    buffer[0] = NUMBER_OF_PACKETS;
    //fprintf(stderr, "buffer[0] = %d\n", buffer[0]);

    if (size % effective_msg_length == 0)
    {
        size = size / effective_msg_length;
    }
    else
    {
        size = size / effective_msg_length + 1;
    }

    buffer[1] = size & 0xff;
    buffer[2] = (size >> 8) & 0xff;

    fprintf(stderr,"Send pack %d\n", buffer[0]);
    if (send(connfd, buffer, MAX_SIZE, 0) == -1)
    {
        fprintf(stderr, "Error while sending the data.\n");
        fclose(image);
        fprintf(stderr, "Socket closed\n");
        close(connfd);
        exit(1);
    }

    fprintf(stderr, "Error while reading from socket: %d\n", errno);
    perror(NULL);

    bzero(buffer, MAX_SIZE);
    if (recv(connfd, buffer, MAX_SIZE, 0) == -1)
    {
        fprintf(stderr, "READ CONFIRMATION ERROR\n");
        fclose(image);
        fprintf(stderr, "Socket closed\n");
        close(connfd);
        exit(1);
    }

    fprintf(stderr, "Error while reading from socket: %d\n", errno);
    perror(NULL);

    
    if (buffer[0] != CONFIRMATION)
    {
        fprintf(stderr, "Read something else, instead of CONFIRMATION");
        fclose(image);
        fprintf(stderr, "Socket closed\n");
        close(connfd);
        exit(1);
    }
    bzero(buffer, MAX_SIZE);

    fprintf(stderr, "Sock: %d\n", connfd);

    while((bytes_read = fread(buffer + HEADER_SIZE, sizeof(char), effective_msg_length, image)) > 0)
    {
        //fprintf(stderr,"Send pack 3\n");
        fprintf(stderr,"SCCCCCCc\n");
        buffer[0] = 3;
        buffer[1] = bytes_read & 0xff;
        buffer[2] = (bytes_read >> 8) & 0xff;

        //fprintf(stderr, "%ld\n", ftell(image));
        fprintf(stderr,"Send pack %d\n", buffer[0]);
        if (send(connfd, buffer, bytes_read + HEADER_SIZE, 0) == -1)
        {
            fprintf(stderr, "Error while sending the data.\n");
            fprintf(stderr, "FREE: 1\n");
            free(image_path);
            fclose(image);
            fprintf(stderr, "Socket closed\n");
            close(connfd);
            exit(1);
        }

        if ((bytes_read = (connfd, buffer, MAX_SIZE, 0)) < 0)
        {
            fprintf(stderr, "READ CONFIRMATION ERROR\n");
            fclose(image);
            fprintf(stderr, "Socket closed\n");
            close(connfd);
            exit(1);
        }

        if (buffer[0] != CONFIRMATION && bytes_read > 0)
        {
            fprintf(stderr, "Read something else, instead of CONFIRMATION");
            fclose(image);
            fprintf(stderr, "Socket closed\n");
            close(connfd);
            exit(1);
        }
        bzero(buffer, MAX_SIZE);
    }
    fprintf(stderr, "Sock: %d\n", connfd);
    if (bytes_read < 0)
    {
            fprintf(stderr, "Error while reading from socket: %d", errno);
            perror(NULL);
    }

    fclose(image);
    //fprintf(stderr, "Socket closed\n");
    //close(connfd);
}

void* serve_client(void* conn)
{
    int connfd = *(int*)conn;
    char buffer[MAX_SIZE];
    ssize_t bytes_read;
    char *image_path = (char *) malloc (128);
    FILE *image;


    // Request request: type, size, data
    int number_of_packets = 0;
    Request request;
    request.message = NULL;

    while((bytes_read = recv(connfd, buffer, MAX_SIZE, 0)) > 0)
    {
        int confirmation = 0;
        request.message_type = buffer[0];
        //fprintf(stderr, "Message type: %d , buffer[0] - 48 = %d\n", request.message_type, buffer[0] - 48);
        switch (request.message_type){
            case CONFIRMATION:
                //fprintf(stderr, "Confirmation received\n");
                if (request.message != NULL)
                {
                    fprintf(stderr, "FREE: 2\n");
                    free(request.message);
                    request.message = NULL;
                }
                break;
            case NUMBER_OF_PACKETS:
                //fprintf(stderr, "Number of packets received\n");
                if (request.message != NULL)
                {
                    fprintf(stderr, "FREE: 3\n");
                    free(request.message);
                }
                number_of_packets = 0xff & buffer[1];
                number_of_packets |= (0xff & buffer[2]) << 8;

                snprintf(image_path, 128, "files/client%dimage%d.png", connfd, image_number++);

                image = fopen(image_path, "wb");

                confirmation = 1;
                break;
            case PACKET:
                //fprintf(stderr, "Packet received\n");
                if (request.message != NULL)
                {
                    fprintf(stderr, "FREE: 4\n");
                    free(request.message);
                }
                request.length = 0xff & buffer[1];
                request.length |= (0xff & buffer[2]) << 8;
                request.message = (char*) malloc (request.length);
                memcpy(request.message, buffer + HEADER_SIZE, request.length);
                confirmation = 1;
                break;
        }

        if (confirmation && number_of_packets > 1)
        {
            //fprintf(stderr, "Sending confirmation\n");
            bzero(buffer, MAX_SIZE);
            buffer[0] = CONFIRMATION;
            fprintf(stderr,"Send pack %d\n", buffer[0]);
            send(connfd, buffer, 1, 0);
            //fprintf(stderr, "Confirmation sent\n");
        }

        if (request.message != NULL)
        {
            //fprintf(stderr, "Writing image\n");
            fwrite(request.message, 1, request.length, image);
            //fprintf(stderr, "Writing imageeeeeeeeeeeee\n");
            number_of_packets--;
            if (number_of_packets == 0)
            {
                fclose(image);
                process(image_path);
                fprintf(stderr, "Hello de 2 ori?\n");
                fprintf(stderr, "Sock: %d\n", connfd);
                send_back_image(connfd, image_path);
                
                if (image_path)
                {
                    fprintf(stderr, "FREE: 5\n");
                    //free(image_path);
                }
            }
        }
        bzero(buffer, MAX_SIZE);
    }

    if (bytes_read <= 0)
    {
            fprintf(stderr, "Error while reading from socket: %d", errno);
            perror(NULL);
    }

    printf("client served\n");
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] == connfd)
        {
            client_sockets[i] = 0;
            break;
        }
    }

    fprintf(stderr, "leaving client\n");
    fprintf(stderr, "Socket closed\n");
    close(connfd);
}