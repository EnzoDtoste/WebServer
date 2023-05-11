#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>

#define BUFFER_SIZE 10240
#define TRUE 1
#define FALSE 0

struct nodo {
    struct stat st;
    struct dirent* entry;
    struct nodo *sig;
};

struct nodo* create_list(struct dirent* entry, struct stat st)
{
    struct nodo *nuevo;
    nuevo=malloc(sizeof(struct nodo));
    nuevo->st = st;
    nuevo->entry = entry;
    nuevo->sig = NULL;
    return nuevo;
}

void insertSize(struct nodo* raiz, struct dirent* entry, struct stat st, int asc)
{   
    struct stat rst = raiz->st;
    if(asc == TRUE && rst.st_size < st.st_size)
    {
        if(raiz->sig == NULL)
        {
            struct nodo *nuevo;
            nuevo=malloc(sizeof(struct nodo));
            nuevo->st = st;
            nuevo->entry = entry;
            nuevo->sig = NULL;
            raiz->sig = nuevo;
        }
        else
            insertSize(raiz->sig, entry, st, asc);
    }
    else if(asc == FALSE && rst.st_size > st.st_size)
    {
        if(raiz->sig == NULL)
        {
            struct nodo *nuevo;
            nuevo=malloc(sizeof(struct nodo));
            nuevo->st = st;
            nuevo->entry = entry;
            nuevo->sig = NULL;
            raiz->sig = nuevo;
        }
        else
            insertSize(raiz->sig, entry, st, asc);
    }
    else
    {
        struct nodo *temp;
        temp=malloc(sizeof(struct nodo));
        temp->st = raiz->st;
        temp->entry = raiz->entry;
        temp->sig = raiz->sig;

        raiz->entry = entry;
        raiz->st = st;
        raiz->sig = temp;
    }   
}

void insertName(struct nodo* raiz, struct dirent* entry, struct stat st, int asc)
{   
    if(asc == TRUE && strcasecmp(raiz->entry->d_name, entry->d_name) <= -1)
    {
        if(raiz->sig == NULL)
        {
            struct nodo *nuevo;
            nuevo=malloc(sizeof(struct nodo));
            nuevo->st = st;
            nuevo->entry = entry;
            nuevo->sig = NULL;
            raiz->sig = nuevo;
        }
        else
            insertName(raiz->sig, entry, st, asc);
    }
    else if(asc == FALSE && strcasecmp(raiz->entry->d_name, entry->d_name) >= 1)
    {
        if(raiz->sig == NULL)
        {
            struct nodo *nuevo;
            nuevo=malloc(sizeof(struct nodo));
            nuevo->st = st;
            nuevo->entry = entry;
            nuevo->sig = NULL;
            raiz->sig = nuevo;
        }
        else
            insertName(raiz->sig, entry, st, asc);
    }
    else
    {
        struct nodo *temp;
        temp=malloc(sizeof(struct nodo));
        temp->st = raiz->st;
        temp->entry = raiz->entry;
        temp->sig = raiz->sig;

        raiz->entry = entry;
        raiz->st = st;
        raiz->sig = temp;
    }   
}

void freeNodo(struct nodo* raiz)
{
    struct nodo *reco = raiz;
    struct nodo *bor;
    while (reco != NULL)
    {
        bor = reco;
        reco = reco->sig;
        free(bor);
    }
}

char* init_path;

void send_response(int client_socket, char* response, int response_length) {
    char headers[BUFFER_SIZE];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", response_length);
    send(client_socket, headers, strlen(headers), 0);
    send(client_socket, response, response_length, 0);
}

void send_directory_listing(int client_socket, char* directory_path) {
    struct nodo* folders = NULL;
    struct nodo* files = NULL;

    DIR* directory = opendir(directory_path);
    struct dirent* entry;

    while ((entry = readdir(directory)) != NULL) {
        struct stat st;
        char name[BUFFER_SIZE];
        sprintf(name, "%s%s", directory_path, entry->d_name);
        if (stat(name, &st) == 0) {
            if (S_ISDIR(st.st_mode))
            {
                if(folders == NULL)
                    folders = create_list(entry, st);
                else
                    insertName(folders, entry, st, TRUE);
            }
            else
            {
                if(files == NULL)
                    files = create_list(entry, st);
                else
                    insertName(files, entry, st, TRUE);
            }
        }
        else
        {
            if(files == NULL)
                files = create_list(entry, st);
            else
                insertName(files, entry, st, TRUE);
        }
    }

    char response[BUFFER_SIZE];
    int response_length = 0;

    response_length += sprintf(response + response_length, "<html><head><title>Directorio %s</title></head><body><table><tr><th>Name</th><th>Size</th><th>Last Modification Date</th></tr>", directory_path);

    struct nodo* folder = folders;
    while(folder != NULL)
    {
        response_length += sprintf(response + response_length, "<tr><td><a href=\"%s/\">%s</a></td><td></td><td>%s</td></tr>", folder->entry->d_name, folder->entry->d_name, ctime(&(folder->st).st_mtime));
        folder = folder->sig;
    }
    freeNodo(folders);

    struct nodo* file = files;
    while(file != NULL)
    {
        response_length += sprintf(response + response_length, "<tr><td><a href=\"%s/\" download=\"%s\">%s</a></td><td>%ld</td><td>%s</td></tr>", file->entry->d_name, file->entry->d_name, file->entry->d_name, (file->st).st_size, ctime(&(file->st).st_mtime));
        file = file->sig;
    }
    freeNodo(files);

    response_length += sprintf(response + response_length, "</table></body></html>");

    closedir(directory);
    send_response(client_socket, response, response_length);
}

void send_file(int client_socket, char* file_path) {
    file_path[strlen(file_path) - 1] = '\0';
    int file_descriptor = open(file_path, O_RDONLY);
    if (file_descriptor == -1) {
        send_response(client_socket, "File not found", strlen("File not found"));
        return;
    }
    struct stat buf;
    fstat (file_descriptor, &buf);   

    char headers[BUFFER_SIZE];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", buf.st_size);
    send(client_socket, headers, strlen(headers), 0);

    sendfile(client_socket, file_descriptor, 0, buf.st_size);
    close(file_descriptor);
}

void handle_request(int client_socket, char* request) {
    char method[BUFFER_SIZE];
    char path[BUFFER_SIZE];
    sscanf(request, "%s %s", method, path);
    if (strcmp(method, "GET") != 0) {
        send_response(client_socket, "Method not allowed", strlen("Method not allowed"));
        return;
    }
    int len1 = strlen(init_path);
    int len2 = strlen(path);
    char newPath[len1 + len2];
    sprintf(newPath, "%s%s", init_path, path);
    newPath[len1 + len2] = '\0';
    struct stat st;
    if (stat(newPath, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            send_directory_listing(client_socket, newPath);
        }
        else {
            send_file(client_socket, newPath);
        }
    }
    else {
        send_file(client_socket, newPath);
    }
}

void* handle_client(void* arg) {
    int client_socket = *((int*)arg);
    char buffer[BUFFER_SIZE];
    int bytes_received;
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    buffer[bytes_received] = '\0';
    handle_request(client_socket, buffer);
    close(client_socket);
    free(arg);
    return NULL;
}

int main(int argc, char **argv) {
    if(argc >= 3)
    {
        int PORT = atoi(argv[1]);
        init_path = argv[2];

        int server_socket, client_socket;
        struct sockaddr_in server_address, client_address;
        socklen_t address_length = sizeof(client_address);
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            printf("Error creating socket\n");
            return -1;
        }
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = INADDR_ANY;
        server_address.sin_port = htons(PORT);
        if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
            printf("Error binding socket\n");
            return -1;
        }
        if (listen(server_socket, 10) == -1) {
            printf("Error listening on socket\n");
            return -1;
        }
        
        printf("Server listening on port %d\n", PORT);
        while (1) {
            client_socket = accept(server_socket, (struct sockaddr*)&client_address, &address_length);
            if (client_socket == -1) {
                printf("Error accepting connection\n");
                continue;
            }
            int* arg = malloc(sizeof(int));
            *arg = client_socket;
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, arg) != 0) {
                printf("Error creating thread\n");
                continue;
            }
            pthread_detach(thread);
        }
    }
    else
        printf("You must specify port and path\n");
    return 0;
}