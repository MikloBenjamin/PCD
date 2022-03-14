#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <errno.h>

#define MAX_SIZE 1024
#define MAX_REQUESTS 100
#define HEADER_SIZE 3
#define PORT 7366
#define ADDR "127.0.0.1"

pthread_mutex_t mutex_buffer;

sem_t sem_empty;                // Semafor care tine evidenta cate locuri libere sunt in coada de mesaje
sem_t sem_full;                 // Semafor care tine evidenta cate locuri ocupate sunt in coada de mesaje
sem_t sem_send_package;         // Semafor care asteapta dupa confirmarea de primire de pachet
sem_t sem_terminate_processes;  // Semafor care se face 1 atunci cand toate pozele au fost prelucrate, adica cand total_images == 0

struct thread_parameters
{
    char *path;
    int socket_fd;
};

typedef struct Request
{
    int message_type;       // X
    unsigned length;        // Y
    char *message;          // Z
} Request;

Request requests[MAX_REQUESTS];
int length_of_requests = 0;
int total_images = 0;

void read_and_send_image(char *dir_path, char *image_name, int socket_fd)
{
    // Buffer-ul in care se vor citi bucatile imagini
    char buffer[MAX_SIZE];
    // Calea absoluta spre imagine
    char *image_path = (char *)malloc((strlen(dir_path) + strlen(image_name) + 2) * sizeof(char));
    size_t bytes_read;
    int effective_msg_length = MAX_SIZE - HEADER_SIZE;      // Dimensiunea efectiva a mesajului, fara header, doar datele pure
    long int size;

    // Construim calea completa
    strcpy(image_path, dir_path);
    strcat(image_path + strlen(dir_path), "/");
    strcat(image_path + strlen(dir_path) + 1, image_name);

    // Deschidem imaginea in modul de rb - read bytes
    FILE* img_descriptor = fopen(image_path, "rb");

    if (!img_descriptor)
    {
        fprintf(stderr, "Could not open image for reading: %s\n", dir_path);
        free(image_path);
        exit(2);
    }

    /*---------------------------------------------------------------------------------------
    TRIMITEREA PACHETULUI CARE INDICA NUMARUL TOTAL DE PACHETE CARE URMEAZA SA FIE TRIMISE
    ---------------------------------------------------------------------------------------*/

    fseek(img_descriptor, 0, SEEK_END);         // seek to end of file
    size = ftell(img_descriptor);               // get current file pointer
    fseek(img_descriptor, 0, SEEK_SET);         // seek back to beginning of file

    buffer[0] = 2;          // Anunta pachet cu nr de pachete
    if (size % effective_msg_length == 0)
    {
        size = size / effective_msg_length;
    }
    else
    {
        size = size / effective_msg_length + 1;
    }

    buffer[1] = size & 0xff;          // Izolam primul byte din bytes_read
    buffer[2] = (size >> 8) & 0xff;   // Izolam al doilea byte din bytes_read

    sem_wait(&sem_send_package);
    fprintf(stderr,"Send pack 2 %d\n", socket_fd);
    // DACA PUNEM SI CALEA SA ACTUALIZAM SI IN WRITE CU PATH_LENGTH + HEADER_SIZE
    if (send(socket_fd, buffer, MAX_SIZE, 0) == -1)
    {
        fprintf(stderr, "Error while sending the data.\n");
        free(image_path);
        fclose(img_descriptor);
        close(socket_fd);
        exit(4);
    }

    /*---------------------------------------------------------------------------------------
    CITIREA SI TRIMITEREA PACHETELOR CU POZA EFECTIVA
    ---------------------------------------------------------------------------------------*/

    // fread() intoarce numarul de bucati citite (adica ce punem in al treilea argument), in cazul in care intoarce 0 inseamna ca am ajuns la capat
    // fiindca nu se mai citeste nimic
    // Fiecare bucata citita e trimisa serverului
    FILE *tmp = fopen("/home/mihai/Desktop/output/bb.png", "wb");
    while((bytes_read = fread(buffer + HEADER_SIZE, sizeof(char), effective_msg_length, img_descriptor)) == effective_msg_length)
    {
        fprintf(stderr,"Send pack 3\n");
        buffer[0] = 3;                          // Tip de mesaj: trimitere pachet de date
        buffer[1] = bytes_read & 0xff;          // Izolam primul byte din bytes_read
        buffer[2] = (bytes_read >> 8) & 0xff;   // Izolam al doilea byte din bytes_read

        fprintf(stderr, "prim %ld\n", bytes_read);

        fwrite(buffer + HEADER_SIZE, sizeof(char), bytes_read, tmp);    // scrierea de verificare, se sterge dupa

        fprintf(stderr, "%ld\n", ftell(img_descriptor));

        // Asteptam confirmarea de la server ca a primit pachetul
        sem_wait(&sem_send_package);

        if (send(socket_fd, buffer, bytes_read + HEADER_SIZE, 0) == -1)
        {
            fclose(tmp);

            fprintf(stderr, "Error while sending the data.\n");
            free(image_path);
            fclose(img_descriptor);
            close(socket_fd);
            exit(4);
        }
    }

    if (feof(img_descriptor))
    {
        fwrite(buffer + HEADER_SIZE, sizeof(char), bytes_read, tmp);

        buffer[0] = 3;                          // Tip de mesaj: trimitere pachet de date
        buffer[1] = bytes_read & 0xff;          // Izolam primul byte din bytes_read
        buffer[2] = (bytes_read >> 8) & 0xff;   // Izolam al doilea byte din bytes_read

        fprintf(stderr, "%ld\n", ftell(img_descriptor));
        sem_wait(&sem_send_package);

        if (send(socket_fd, buffer, bytes_read + HEADER_SIZE, 0) == -1)
        {
            fclose(tmp);

            fprintf(stderr, "Error while sending the data.\n");
            free(image_path);
            fclose(img_descriptor);
            close(socket_fd);
            exit(4);
        }
    }

    fclose(tmp);
    free(image_path);
    fclose(img_descriptor);
}

int is_png(char *name)
{
    if (strcmp(name + strlen(name) - 4, ".png") == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void set_number_of_total_images(char *path)
{
    DIR *dir_descriptor;
    struct dirent *dir;
    dir_descriptor = opendir(path);

    if (dir_descriptor != NULL)
    {
        while ((dir = readdir(dir_descriptor)) != NULL)
        {
            // Alegem doar fisierele regulare, doar acelea pot reprezenta potentiale poze
            if (dir->d_type == DT_REG && is_png(dir->d_name))
            {
                pthread_mutex_lock(&mutex_buffer);
                total_images++;
                pthread_mutex_unlock(&mutex_buffer);
            }
        }

        pthread_mutex_lock(&mutex_buffer);
        total_images--;
        pthread_mutex_unlock(&mutex_buffer);

        //fprintf(stderr,"%d\n", total_images);
        closedir(dir_descriptor);
    }
    else
    {
        fprintf(stderr, "Could not open path: %s\n", path);
        exit(1);
    }
}

void *find_images(void *args)
{
    //int socket_fd;
    struct thread_parameters *parameters = (struct thread_parameters *)args;
    DIR *dir_descriptor;
    struct dirent *dir;
    dir_descriptor = opendir(parameters->path);

    printf("Thread: %s\n", parameters->path);

    // Initializam numarul total de poze - 1
    set_number_of_total_images(parameters->path);

    if (dir_descriptor != NULL)
    {   
        while ((dir = readdir(dir_descriptor)) != NULL)
        {
            // Alegem doar fisierele regulare, doar acelea pot reprezenta potentiale poze
            if (dir->d_type == DT_REG && is_png(dir->d_name))
            {
                fprintf(stderr,"%s", dir->d_name);
                read_and_send_image(parameters->path, dir->d_name, parameters->socket_fd);
            }
        }

        closedir(dir_descriptor);
    }
    else
    {
        fprintf(stderr, "Could not open path: %s\n", parameters->path);
        exit(1);
    }

    pthread_exit(0);
}

int establish_connection()
{
    int socket_fd, conn_fd;
    struct sockaddr_in server_addr;

    // socket create and verification
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        printf("Socket creation has failed.\n");
        exit(2);
    }

    bzero(&server_addr, sizeof(server_addr));

    // assign IP, PORT
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ADDR);
    server_addr.sin_port = htons(PORT);

    // connect the client socket to server socket
    if (connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0)
    {
        printf("The connection with the server has failed.\n");
        exit(3);
    }

    return socket_fd;
}

void process_request(Request request, int socket_fd)
{
    // Buffer-ul in care se vor citi bucatile imagini
    char buffer[MAX_SIZE];
    FILE *processed_img_handler;
    int total_packages;
    int current_package;

    if (request.message_type)
        fprintf(stderr, "Msg: %d\n", request.message_type);

    switch (request.message_type)
    {
    case 1:         // Confirmare de primire pachet
        sem_post(&sem_send_package);
        break;

    case 2:         // Pachet cu informatii despre poza care urmeaza sa fie trimisa
        processed_img_handler = fopen("/home/mihai/Desktop/output/bb.png", "wb");

        if (!processed_img_handler)
        {
            fprintf(stderr, "Could not open image for reading: \n");
            //free(image_path); 
            close(socket_fd);
            //free(request.message); DE DECOMENTAT DOAR DACA PUNEM SI NUMELE IN PACHET
            exit(2);
        }

        total_packages = request.length;
        current_package = 0;

        // Trimitere confirmare de primire pachet
        buffer[0] = 1;

        if (send(socket_fd, buffer, 1, 0) == -1)
        {
            fprintf(stderr, "Error while sending the data.\n");
            //free(request.message); DE DECOMENTAT DOAR DACA PUNEM SI NUMELE IN PACHET
            fclose(processed_img_handler);
            close(socket_fd);
            exit(4);
        }

        break;

    case 3:         // Pachet cu date din poza

        if (current_package < total_packages)
        {
            if (fwrite(request.message, sizeof(char), request.length, processed_img_handler) == -1)
            {
                fprintf(stderr, "Error while writing the processed image.\n");
                fclose(processed_img_handler);
                free(request.message);
                exit(5);
            }

            current_package++;

            // Daca am ajuns la ultimul pachet, inchidem fisierul fiindca s-a primit toata poza si
            // incrementam semaforul pentru a citi mai departe urmatoarea poza
            if (current_package == total_packages)
            {
                fclose(processed_img_handler);
                sem_post(&sem_send_package);

                // Dupa fiecare poza prelucrata decrementam total_images, fiindca atunci cand ajunge la 0 inseamna ca am procesat tot
                pthread_mutex_lock(&mutex_buffer);
                total_images--;
                pthread_mutex_unlock(&mutex_buffer);
            }
            else    // Pentru ultimul pachet nu mai trimitem confirmare
            {
                // Trimitere confirmare de primire pachet
                buffer[0] = 1;

                if (send(socket_fd, buffer, 1, 0) == -1)
                {
                    fprintf(stderr, "Error while sending the data.\n");
                    free(request.message);
                    fclose(processed_img_handler);
                    close(socket_fd);
                    exit(4);
                }
            }
        }

        free(request.message);

        break;

    default:
        break;
    }

    pthread_mutex_lock(&mutex_buffer);
    if (total_images == -1)
    {
        sem_post(&sem_terminate_processes);
    }
    pthread_mutex_unlock(&mutex_buffer);
}

void *manage_requests(void *args)
{
    Request request;
    int socket_fd;

    socket_fd = *((int *)args);

    while (1)
    {
        sem_wait(&sem_full);   // Daca nu avem locuri libere in coada, asteptam pana se fac
        pthread_mutex_lock(&mutex_buffer);
        request = requests[0];
        //printf("%d\n", length_of_requests);
        for (int i = 0; i < length_of_requests; i++)
        {
            requests[i] = requests[i + 1];
        }
        length_of_requests--;
        pthread_mutex_unlock(&mutex_buffer);
        sem_post(&sem_empty);    // Cand apare un loc liber, atunci incrementam cate locuri pline sunt, ptr ca am adaugat un element

        process_request(request, socket_fd);   // Trimitem mesajul la procesat
    }
}

void *read_requests(void *args)
{
    char buffer[MAX_SIZE];
    int socket_fd = *((int *)args);
    int effective_msg_size = MAX_SIZE - HEADER_SIZE;

    while (1)
    {
        Request request;

        int i = recv(socket_fd, buffer, MAX_SIZE, 0);

        if (i == -1)
        {
        fprintf(stderr, "%d", errno);
        perror(NULL);
        }

        request.message_type = buffer[0];               // Dam X
        request.length = 0xff & buffer[1];                 // Primul byte din Y
        request.length |= (0xff & buffer[2]) << 8;       // Al doilea byte din Y

        if (request.message_type == 3)      // Doar mesajele de tip 3 au un Z cu continut
        {
            request.message = (char*)malloc(sizeof(char) * request.length);     // Dam Z
            memcpy(request.message, buffer + HEADER_SIZE, request.length);
        }

        sem_wait(&sem_empty);   // Daca nu avem locuri libere in coada, asteptam pana se fac
        pthread_mutex_lock(&mutex_buffer);
        requests[length_of_requests++] = request;
        //printf("%d\n", length_of_requests - 1);
        pthread_mutex_unlock(&mutex_buffer);
        sem_post(&sem_full);    // Cand apare un loc liber, atunci incrementam cate locuri pline sunt, ptr ca am adaugat un element

        //process_request(request, socket_fd);   // Trimitem mesajul la procesat
    }

    pthread_exit(0);
}

int main(int argc, char* argv[])
{
    static struct option long_options[] =
    {
        {"path", required_argument, 0, 'p'},
        {0, 0, 0, 0}    // Pe ultimul rand din optiuni trebuie sa apara neaparat {0, 0, 0, 0}
    };

    int option;
    int option_index = 0;
    int socket_fd;

    pthread_t sender_thread;                // Trimite imaginile serverului
    pthread_t reader_thread;                // Citeste mesajele care vin de la server
    pthread_t requests_manager_thread;      // Extrage mesajele din coada de mesaje si le proceseaza
    struct thread_parameters parameters;

    pthread_mutex_init(&mutex_buffer, NULL);
    sem_init(&sem_empty, 0, MAX_REQUESTS);      // Initial toate locurile sunt libere in coada de mesaje
    sem_init(&sem_full, 0, 0);                  // Initial niciun loc nu e ocupat in coada de mesaje
    sem_init(&sem_send_package, 0, 1);          // Initial putem trimite un pachet
    sem_init(&sem_terminate_processes, 0, 0);   // Initial putem trimite un pachet

    // Obtinem socket-ul ca sa comunicam cu serverul
    socket_fd = establish_connection();

    pthread_create(&reader_thread, NULL, read_requests, &socket_fd);

    // DE PUS IF CARE VERIFICA NUMARUL DE ARGUMENTE
    // DE PORNIT THREAD PENTRU PROCESARE COADA DE MESAJE

    // Faptul ca exista acele : dupa litere, gen c: sau f: sau k: anunta ca urmeaza un argument dupa
    while ((option = getopt_long(argc, argv, "p:", long_options, &option_index)) != -1)
    {
        switch (option)
        {
        case 'p':
            parameters.socket_fd = socket_fd;
            parameters.path = (char *)malloc(strlen(optarg) * sizeof(char) + 1);
            strcpy(parameters.path, optarg);

            pthread_create(&requests_manager_thread, NULL, manage_requests, &socket_fd);
            pthread_create(&sender_thread, NULL, find_images, &parameters);
            break;

        default:
            // De pus usage
            // printf("Usage: --celsius <double value> and/or --fahrenheit <double value> and/or --kelvin <double value>.\n");
            exit(2);
        };
    }

    pthread_join(sender_thread, NULL);
    //pthread_join(requests_manager_thread, NULL);
    //pthread_join(reader_thread, NULL);

    sem_wait(&sem_terminate_processes);
    pthread_cancel(requests_manager_thread);
    pthread_cancel(reader_thread);

    // close the socket
    close(socket_fd);
    free(parameters.path);
    pthread_mutex_destroy(&mutex_buffer);
    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);
    sem_destroy(&sem_send_package);

    return 0;
}