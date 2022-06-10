#include "server.h"

#define MAX_REQUESTS 100
#define MAX_SIZE 1024
#define INFO_SIZE 5
#define CLIENT_INFO_SIZE 11
#define HEADER_SIZE 3
#define MAX_CLIENTS 50
#define ADDR "127.0.0.1"
#define SA struct sockaddr

typedef enum OptionType {
    NEGATIV = 0,
    SEPIA = 1,
    BLUR = 2,
    BLACK_AND_WHITE = 3,
    DEFAULT = 4
} Option;

typedef enum MessageType{
    NR_IMAGES_AND_FILTERS = 0,
    CONFIRMATION = 1,
    NUMBER_OF_PACKETS = 2,
    PACKET = 3,
    ERROR = 4
} MessageType;

typedef enum AdminMessageType{
    DISCONNECT = 0,
    END = 1,
    INFO = 2,
    KILL_CLIENT_BY_THREAD_ID = 3,
    KILL_CLIENT_BY_SOCKET_ID = 4
} AdminMessageType;

typedef struct Client
{
    Option option;
    int number_of_images;
    int socket_id;
} Client;

typedef struct Request
{
    MessageType message_type;
    unsigned length;
    char *message;
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

int client_sockets[MAX_CLIENTS] = {};
time_t client_start[MAX_CLIENTS] = {};
Client* clients[MAX_CLIENTS] = {};

int admins = 0, waiting_clients = 1;
struct sigaction signal_handler;

time_t start_t;

void handle_sigint(int sig_number)
{
    waiting_clients = 0;
    sem_post(&end_server);
}

void* read_admin(void* conn);
void* wait_admin(void* param);

void* wait_for_clients(void* server);
void* serve_client(void* conn);

int create_files_directory_as_needed();

void* run_server(void* port)
{
    time(&start_t);
    int PORT = *(int*)port;

    signal(SIGINT, handle_sigint);

    int opt = 1;     int sockfd;
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

    if ((listen(server_socket.socket_id, MAX_CLIENTS)) != 0) {
        printf("!!! Server Failed to start listening !!!\n");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Server is listening on port: %d!\n", PORT);
    }
    sem_init(&end_server, 0, 0);

    if (create_files_directory_as_needed() < 0)
    {
        fprintf(stderr, "Error creating 'files' directory\n");
        exit(1);
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        client_sockets[i] = 0;
        clients[i] = NULL;
    }

    pthread_create(&wait_clients, NULL, wait_for_clients, &server_socket);
    pthread_create(&wait_admins, NULL, wait_admin, NULL);

    printf("Waiting for admin to end the server\n");
    sem_wait(&end_server);
    printf("Ending server\n");

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != NULL)
        {
            close(client_sockets[i]);
            pthread_cancel(client_threads[i]);
            client_start[i] = 0;
            free(clients[i]);
            clients[i] = NULL;
        }
    }
    printf("end main\n");
    return EXIT_SUCCESS;
}

void send_info(int admin_connection)
{
    /// nr_clients up_time
    /// 0   1      2    3

    char info[1024];

    int nr_clients = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != NULL)
        {
            nr_clients++;
        }
    }

    info[0] = nr_clients & 0xff;
    info[1] = (nr_clients >> 8) & 0xff;

    time_t up_t;
    time(&up_t);

    int uptime = (int)difftime(up_t, start_t);
    info[2] = uptime & 0xff;
    info[3] = (uptime >> 8) & 0xff;

    if (send(admin_connection, info, 1024, 0) == -1)
    {
        fprintf(stderr, "Error while sending the data.\n");
        close(admin_connection);
        exit(1);
    }

    /// client_socket_id client_thread_id client_uptime client_nr_images client_option
    /// 0        1        2     3        4       5       6        7         8   9
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != NULL)
        {
            int socket_id = client_sockets[i];
            pthread_t thread_id = client_threads[i];

            time_t end_t;
            time(&end_t);
            double upt = difftime(end_t, client_start[i]);

            int nr_images = clients[i]->number_of_images;
            int option = clients[i]->option;

            snprintf(info, 1024, "%d|%ld|%f|%d|%d", socket_id, thread_id, upt, nr_images, option);

            if (send(admin_connection, info, 1024, 0) == -1)
            {
                fprintf(stderr, "Error while sending the data.\n");
                close(admin_connection);
                exit(1);
            }
        }
    }

}


void* read_admin(void* conn)
{
    int connfd = *(int*)conn;
    char buff[128];
    ssize_t bread;
    printf("Admin thread started\n");
    char *number;

    while(admins == 1 && (bread = read(connfd, buff, sizeof(buff))) > 0)
    {
        switch (buff[0] - 48)
        {
            case DISCONNECT:
                fprintf(stderr, "Admin DISCONNECT\n");
                admins = 0;
                break;
            case END:
                fprintf(stderr, "Admin END server\n");
                admins = -1;
                sem_post(&end_server);
                break;
            case INFO:
                fprintf(stderr, "Admin GET info\n");
                send_info(connfd);
                break;
            case KILL_CLIENT_BY_THREAD_ID:
                fprintf(stderr, "Admin KILL client\n");
                number = strtok(buff, " ");
                number = strtok(NULL, " ");

                unsigned long int thread_id = 0;

                for (int i = 0; i < strlen(number) - 1; i++)
                {
                    thread_id = thread_id * 10 + (number[i] - '0');
                }

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_threads[i] == thread_id)
                    {
                        pthread_cancel(thread_id);
                        close(client_sockets[i]);
                        client_sockets[i] = 0;
                        client_start[i] = 0;
                        free(clients[i]);
                        clients[i] = NULL;
                    }
                }
                break;
            case KILL_CLIENT_BY_SOCKET_ID:
                fprintf(stderr, "Admin KILL client\n");
                number = strtok(buff, " ");
                number = strtok(NULL, " ");

                int socket_id = 0;

                for (int i = 0; i < strlen(number) - 1; i++)
                {
                    socket_id = socket_id * 10 + (number[i] - '0');
                }

                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (client_sockets[i] == socket_id)
                    {
                        pthread_cancel(client_threads[i]);
                        close(socket_id);
                        client_sockets[i] = 0;
                        client_start[i] = 0;
                        free(clients[i]);
                        clients[i] = NULL;
                    }
                }
                break;
            default:
                break;
        }
        bzero(buff, sizeof(buff));
    }
    close(connfd);
    printf("Admin thread ended\n");
}

void* wait_admin(void* param)
{
    int PORT = 9375;

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
   
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed for admin...\n");
        exit(0);
    }
    else
        printf("Socket successfully created for admin..\n");
    bzero(&servaddr, sizeof(servaddr));
   
        servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
   
        if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed for admin...\n");
        exit(0);
    }
    else
        printf("Socket successfully binded for admin..\n");
   
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

        int sockfd = accept(
            server_socket->socket_id,
            (struct sockaddr *) &server_socket->socket_address, 
            &len);
        if(sockfd > 0)
        {
            int i;
            for (i = 0; i < MAX_CLIENTS; i++)
            {
                if (client_sockets[i] == 0)
                {
                    Client *client = (Client *) malloc (sizeof(Client) + 1);
                    time_t start_t;
                    time(&start_t);

                    client_sockets[i] = sockfd;
                    client_start[i] = start_t;

                    client->socket_id = sockfd;
                    clients[i] = client;

                    pthread_create(&client_threads[i], NULL, serve_client, client);
                    break;
                }
            }
        }
        else
        {
            fprintf(stderr, "Client connect failed, socket < 0: %d\n", sockfd);
        }
    }
}

typedef struct png_data_struct
{
    int x, y;

    int width, height;
    png_byte color_type;
    png_byte bit_depth;

    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep * row_pointers;
} png_data_struct;

void abort_(const char * s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
    abort();
}

void read_png_file(char* image_path, png_data_struct* png)
{
    char header[8];    /* 8 is the maximum size that can be checked */

    /* open file and test for it being a png */
    FILE *fp = fopen(image_path, "rb");

    if (!fp)
    {
        abort_("[read_png_file] File %s could not be opened for reading", image_path);
    }
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8))
            abort_("[read_png_file] File %s is not recognized as a PNG file", image_path);

    /* initialize stuff */
    png->png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png->png_ptr)
            abort_("[read_png_file] png_create_read_struct failed");


    png->info_ptr = png_create_info_struct(png->png_ptr);
    if (!png->info_ptr)
            abort_("[read_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png->png_ptr)))
            abort_("[read_png_file] Error during init_io");

    png_init_io(png->png_ptr, fp);
    png_set_sig_bytes(png->png_ptr, 8);
    png_read_info(png->png_ptr, png->info_ptr);
    png->width = png_get_image_width(png->png_ptr, png->info_ptr);
    png->height = png_get_image_height(png->png_ptr, png->info_ptr);
    png->color_type = png_get_color_type(png->png_ptr, png->info_ptr);
    png->bit_depth = png_get_bit_depth(png->png_ptr, png->info_ptr);
    png->number_of_passes = png_set_interlace_handling(png->png_ptr);
    png_read_update_info(png->png_ptr, png->info_ptr);

    /* read file */
    if (setjmp(png_jmpbuf(png->png_ptr)))
            abort_("[read_png_file] Error during read_image");

    png->row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * png->height);
    for (png->y = 0; png->y < png->height; png->y++)
            png->row_pointers[png->y] = (png_byte*) malloc(png_get_rowbytes(png->png_ptr, png->info_ptr));

    png_read_image(png->png_ptr, png->row_pointers);

    fclose(fp);
}

void write_png_file(char* file_name, png_data_struct* png)
{
    /* create file */
    FILE *fp = fopen(file_name, "wb");
    if (!fp)
        abort_("[write_png_file] File %s could not be opened for writing", file_name);


    /* initialize stuff */
    png->png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png->png_ptr)
        abort_("[write_png_file] png_create_write_struct failed");

    png->info_ptr = png_create_info_struct(png->png_ptr);
    if (!png->info_ptr)
        abort_("[write_png_file] png_create_info_struct failed");

    if (setjmp(png_jmpbuf(png->png_ptr)))
        abort_("[write_png_file] Error during init_io");

    png_init_io(png->png_ptr, fp);


    /* write header */
    if (setjmp(png_jmpbuf(png->png_ptr)))
        abort_("[write_png_file] Error during writing header");

    png_set_IHDR(
        png->png_ptr, png->info_ptr, png->width, png->height,
        png->bit_depth, png->color_type, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE
    );

    png_write_info(png->png_ptr, png->info_ptr);


    /* write bytes */
    if (setjmp(png_jmpbuf(png->png_ptr)))
        abort_("[write_png_file] Error during writing bytes");

    png_write_image(png->png_ptr, png->row_pointers);


    /* end write */
    if (setjmp(png_jmpbuf(png->png_ptr)))
            abort_("[write_png_file] Error during end of write");

    png_write_end(png->png_ptr, NULL);

    /* cleanup heap allocation */
    for (png->y = 0; png->y < png->height; png->y++)
        free(png->row_pointers[png->y]);
    free(png->row_pointers);

    fclose(fp);
}

void process_negativ(png_data_struct* png)
{
    if (png_get_color_type(png->png_ptr, png->info_ptr) == PNG_COLOR_TYPE_RGB)
            abort_("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
                    "(lacks the alpha channel)");

    if (png_get_color_type(png->png_ptr, png->info_ptr) != PNG_COLOR_TYPE_RGBA)
            abort_("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
                    PNG_COLOR_TYPE_RGBA, png_get_color_type(png->png_ptr, png->info_ptr));

    for (png->y = 0; png->y < png->height; png->y++) 
    {
        png_byte* row = png->row_pointers[png->y];

        for (png->x = 0; png->x < png->width; png->x++) 
        {
                png_byte* ptr = &(row[png->x * 4]);
                
                ptr[0] = 255 - ptr[0];
                ptr[1] = 255 - ptr[1];
                ptr[2] = 255 - ptr[2];
        }
    }
}

void process_sepia(png_data_struct* png)
{
    if (png_get_color_type(png->png_ptr, png->info_ptr) == PNG_COLOR_TYPE_RGB)
            abort_("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
                    "(lacks the alpha channel)");

    if (png_get_color_type(png->png_ptr, png->info_ptr) != PNG_COLOR_TYPE_RGBA)
            abort_("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
                    PNG_COLOR_TYPE_RGBA, png_get_color_type(png->png_ptr, png->info_ptr));

    for (png->y = 0; png->y < png->height; png->y++) 
    {
        png_byte* row = png->row_pointers[png->y];

        for (png->x = 0; png->x < png->width; png->x++) 
        {
            png_byte* rgb = &(row[png->x * 4]);
            float red = ((float)rgb[0] * 0.393) + ((float)rgb[1] * 0.769) + ((float)rgb[2] * 0.189);
            float green = (((float)rgb[0] * 0.349) + ((float)rgb[1] * 0.686) + ((float)rgb[2] * 0.168));
            float blue = ((float)rgb[0] * 0.272) + ((float)rgb[1] *0.534) + ((float)rgb[2] * 0.131);
            
            if (red > 255)
                red = 255;
            if (green > 255)
                green = 255;
            if (blue > 255)
                blue = 255;
            
            rgb[0] = (int)(red);
            rgb[1] = (int)(green);
            rgb[2] = (int)(blue);
        }
    }
}

void process_blur(png_data_struct* png)
{
    if (png_get_color_type(png->png_ptr, png->info_ptr) == PNG_COLOR_TYPE_RGB)
            abort_("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
                    "(lacks the alpha channel)");

    if (png_get_color_type(png->png_ptr, png->info_ptr) != PNG_COLOR_TYPE_RGBA)
            abort_("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
                    PNG_COLOR_TYPE_RGBA, png_get_color_type(png->png_ptr, png->info_ptr));

    for (png->y = 0; png->y < png->height; png->y++) 
    {
        png_byte* row0;
        png_byte* row1;
        png_byte* row2;

        if (png->y == 0){
            row0 = (png_byte *) malloc(sizeof(png_bytep) * png->height);
            for (int y = 0; y < png->height; y++)
                row0[y] = 0;
        } else {
            row0 = png->row_pointers[png->y - 1];
        }
        
        row1 = png->row_pointers[png->y];

        if (png->y == png->height - 1){
            row2 = (png_byte*) malloc(sizeof(png_bytep) * png->height);
            for (int y = 0; y < png->height; y++)
                row2[y] = 0;
        } else {
            row2 = png->row_pointers[png->y + 1];
        }

        for (png->x = 0; png->x < png->width; png->x++) 
        {
            png_byte* blured_values = &(row1[png->x * 4]);

            png_byte* rgb0_0;
            png_byte* rgb0_1; 
            png_byte* rgb0_2;

            png_byte* rgb1_0;
            png_byte* rgb1_1;
            png_byte* rgb1_2;

            png_byte* rgb2_0; 
            png_byte* rgb2_1; 
            png_byte* rgb2_2;


            if (png->x == 0){
                rgb0_0 = (png_byte *) malloc(sizeof(png_bytep) * png->width);
                rgb1_0 = (png_byte *) malloc(sizeof(png_bytep) * png->width);
                rgb2_0 = (png_byte *) malloc(sizeof(png_bytep) * png->width);

                for (int y = 0; y < png->height; y++){
                    rgb0_0[y] = 0;
                    rgb1_0[y] = 0;
                    rgb2_0[y] = 0;
                }
            } else {
                rgb0_0 = &(row0[(png->x - 1) * 4]);
                rgb1_0 = &(row1[(png->x - 1) * 4]);
                rgb2_0 = &(row2[(png->x - 1) * 4]);
            }

            rgb0_1 = &(row0[png->x * 4]);
            rgb1_1 = &(row1[png->x * 4]);
            rgb2_1 = &(row2[png->x * 4]);
           
           if (png->x == png->width - 1){
                rgb0_2 = (png_byte *) malloc(sizeof(png_bytep) * png->width);
                rgb1_2 = (png_byte *) malloc(sizeof(png_bytep) * png->width);
                rgb2_2 = (png_byte *) malloc(sizeof(png_bytep) * png->width);

                for (int y = 0; y < png->height; y++){
                    rgb0_2[y] = 0;
                    rgb1_2[y] = 0;
                    rgb2_2[y] = 0;
                }
           } else {
                rgb0_2 = &(row0[(png->x + 1) * 4]);
                rgb1_2 = &(row1[(png->x + 1) * 4]);
                rgb2_2 = &(row2[(png->x + 1) * 4]);
           }

            int new_red_value = (rgb0_0[0] + rgb0_1[0] + rgb0_2[0] +
                        rgb1_0[0] + rgb1_1[0] + rgb1_2[0] +
                        rgb2_0[0] + rgb2_1[0] + rgb2_2[0]) / 9;

            int new_green_value = (rgb0_0[1] + rgb0_1[1] + rgb0_2[1] +
                        rgb1_0[1] + rgb1_1[1] + rgb1_2[1] +
                        rgb2_0[1] + rgb2_1[1] + rgb2_2[1]) / 9;

            int new_blue_value = (rgb0_0[2] + rgb0_1[2] + rgb0_2[2] +
                        rgb1_0[2] + rgb1_1[2] + rgb1_2[2] +
                        rgb2_0[2] + rgb2_1[2] + rgb2_2[2]) / 9;

            blured_values[0] = new_red_value;
            blured_values[1] = new_green_value;
            blured_values[2] = new_blue_value;
        }
    }
}

void process_black_and_white(png_data_struct* png)
{
    if (png_get_color_type(png->png_ptr, png->info_ptr) == PNG_COLOR_TYPE_RGB)
            abort_("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
                    "(lacks the alpha channel)");

    if (png_get_color_type(png->png_ptr, png->info_ptr) != PNG_COLOR_TYPE_RGBA)
            abort_("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
                    PNG_COLOR_TYPE_RGBA, png_get_color_type(png->png_ptr, png->info_ptr));

    for (png->y = 0; png->y < png->height; png->y++) 
    {
        png_byte* row = png->row_pointers[png->y];

        for (png->x = 0; png->x < png->width; png->x++) 
        {
                png_byte* ptr = &(row[png->x * 4]);
                int avg = (ptr[0] + ptr[1] + ptr[2]) / 3;
                ptr[0] = ptr[1] = ptr[2] = avg;
        }
    }
}

void process_image(char* image_path, int option, int image_number, int client_id)
{
    char processed_image_path[128];
    png_data_struct png;
    snprintf(processed_image_path, 128, "files/client%dimage%d.png", client_id, image_number);

    read_png_file(image_path, &png);

    int processed = 1;
    switch(option)
    {
        case NEGATIV:
            process_negativ(&png);
            break;
        case SEPIA:
            process_sepia(&png);
            break;
        case BLUR:
            process_blur(&png);
            break;
        case BLACK_AND_WHITE:
            process_black_and_white(&png);
            break;
        default:
            processed = 0;
    }

    write_png_file(processed_image_path, &png);
}

void send_back_image(int connfd, char* image_path)
{
    char buffer[MAX_SIZE];


    FILE* image = fopen(image_path, "rb");
    if(!image)
    {
        fprintf(stderr, "Could not open image %s!\n", image_path);
        exit(1);
    }
    size_t bytes_read;
    int effective_msg_length = MAX_SIZE - HEADER_SIZE;

    fseek(image, 0, SEEK_END);
    long int size = ftell(image);
    fseek(image, 0, SEEK_SET);

    buffer[0] = NUMBER_OF_PACKETS;
    
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

    if (send(connfd, buffer, MAX_SIZE, 0) == -1)
    {
        fprintf(stderr, "Error while sending the data.\n");
        fclose(image);
        close(connfd);
        exit(1);
    }

    perror(NULL);

    bzero(buffer, MAX_SIZE);
    if (recv(connfd, buffer, MAX_SIZE, 0) == -1)
    {
        fprintf(stderr, "READ CONFIRMATION ERROR\n");
        fclose(image);
        close(connfd);
        exit(1);
    }

    perror(NULL);

    if (buffer[0] != CONFIRMATION)
    {
        fprintf(stderr, "Read something else, instead of CONFIRMATION 1\n");
        fclose(image);
        close(connfd);
        exit(1);
    }
    bzero(buffer, MAX_SIZE);

    while(size > 0)
    {
        bytes_read = fread(buffer + HEADER_SIZE, sizeof(char), effective_msg_length, image);
        buffer[0] = 3;
        buffer[1] = bytes_read & 0xff;
        buffer[2] = (bytes_read >> 8) & 0xff;

        if (send(connfd, buffer, bytes_read + HEADER_SIZE, 0) == -1)
        {
            fprintf(stderr, "Error while sending the data.\n");
            free(image_path);
            fclose(image);
            close(connfd);
            exit(1);
        }

        if ((bytes_read = recv(connfd, buffer, MAX_SIZE, 0)) < 0)
        {
            fprintf(stderr, "READ CONFIRMATION ERROR\n");
            fclose(image);
            close(connfd);
            exit(1);
        }

        if (buffer[0] != CONFIRMATION && bytes_read > 0)
        {
            fprintf(stderr, "Read something else, instead of CONFIRMATION 2\n");
            fclose(image);
            close(connfd);
            exit(1);
        }

        bzero(buffer, MAX_SIZE);
        size--;
    }

    if (bytes_read < 0)
    {
        fprintf(stderr, "Error while reading from socket: %d", errno);
        perror(NULL);
    }

    fclose(image);
}

void* serve_client(void* conn)
{
    Client* client = (Client*)conn;
    int connfd = client->socket_id;
    fprintf(stderr, "Serving client with socket id: %d!\n", connfd);

    char buffer[MAX_SIZE];
    ssize_t bytes_read;
    char *image_path = (char *) malloc (128);
    FILE *image;

    int number_of_packets = 0;
    Request request;
    request.message = NULL;

    int nr_of_images = 0, option = DEFAULT;
    bytes_read = recv(connfd, buffer, MAX_SIZE, 0);
    if (buffer[0] != NR_IMAGES_AND_FILTERS)
    {
        fprintf(stderr, "NOT received number of images and filters pachet ... something else arrived\n");
        exit(1);
    }

    nr_of_images = 0xff & buffer[1];
    nr_of_images |= (0xff & buffer[2]) << 8;

    option = 0xff & buffer[3];
    option |= (0xff & buffer[4]) << 8;

    client->number_of_images = nr_of_images;
    client->option = option;

    bzero(buffer, MAX_SIZE);

    int packet_number = 0;
    while(1)
    {
        bytes_read = recv(connfd, buffer, MAX_SIZE, 0);
        if (bytes_read <= 0){
            fprintf(stderr, "ENDING THE WHILE\n");
            break;
        }
        int confirmation = 0;
        request.message_type = buffer[0];
        switch (request.message_type){
            case CONFIRMATION:
                break;
            case NUMBER_OF_PACKETS:
                number_of_packets = 0xff & buffer[1];
                number_of_packets |= (0xff & buffer[2]) << 8;

                snprintf(image_path, 128, "files/client%dimage%d.png", connfd, nr_of_images);

                image = fopen(image_path, "wb");

                confirmation = 1;
                packet_number = 0;
                break;
            case PACKET:
                request.length = 0xff & buffer[1];
                request.length |= (0xff & buffer[2]) << 8;
                request.message = (char*) malloc (request.length);
                memcpy(request.message, buffer + HEADER_SIZE, request.length);
                confirmation = 1;
                break;
        }

        if (confirmation && number_of_packets > 1)
        {
            bzero(buffer, MAX_SIZE);
            buffer[0] = CONFIRMATION;
            send(connfd, buffer, 1, 0);
        }

        if (request.message != NULL)
        {
            fwrite(request.message, 1, request.length, image);
            number_of_packets--;
            if (number_of_packets == 0)
            {
                fclose(image);
                process_image(image_path, option, nr_of_images, connfd);
                send_back_image(connfd, image_path);
                packet_number = 0;
                nr_of_images--;
    
                if (nr_of_images == 0){
                    break;
                }
            }
            free(request.message);
            request.message = NULL;
        }
        bzero(buffer, MAX_SIZE);
    }

    if (bytes_read < 0)
    {
        fprintf(stderr, "Error while reading from socket: %d", errno);
        perror(NULL);
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_sockets[i] == connfd)
        {
            client_sockets[i] = 0;
            client_start[i] = 0;
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }

    close(connfd);
}

int create_files_directory_as_needed()
{
    DIR* dir = opendir("files");
    if (!dir)
    {
        if (mkdir("files", S_IRWXU | S_IRWXG | S_IRWXO) == -1)
        {
            fprintf(stderr, "Error: %s\n", strerror(errno));
            return -1;
        }
        else
        {
            fprintf(stdout, "'files/' successfully created\n");
        }
    }
    return 0;
}