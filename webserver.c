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
#include <sys/signal.h>
#include <ctype.h>

#define BUFFER_SIZE 102400
#define TRUE 1
#define FALSE 0

enum {
	NAME,
    SIZE,
	DATE
};

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

void insertDate(struct nodo* raiz, struct dirent* entry, struct stat st, int asc)
{   
    double diff = difftime(raiz->st.st_mtime, st.st_mtime);
    
    if(asc == TRUE && diff < 0)
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
            insertDate(raiz->sig, entry, st, asc);
    }
    else if(asc == FALSE && diff > 0)
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
            insertDate(raiz->sig, entry, st, asc);
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
int server_socket;

char* url_to_path(const char* input) {
    int len = strlen(input);
    char* output = (char*)malloc((len + 1) * sizeof(char));
    int i, j = 0;

    for (i = 0; i < len; i++) {
        if (input[i] == '+') {
            output[j++] = ' ';
        } else if (input[i] == '%') {
            if (isxdigit(input[i + 1]) && isxdigit(input[i + 2])) {
                char hex[3] = { input[i + 1], input[i + 2], '\0' };
                output[j++] = (char)strtol(hex, NULL, 16);
                i += 2;
            } else {
                free(output);
                return NULL;
            }
        } else {
            output[j++] = input[i];
        }
    }

    output[j] = '\0';
    return output;
}

char* formatearTamaño (off_t bytes) {
    char* buffer = malloc (20 * sizeof (char));

    double tamaño;
    char* unidad;

    if (bytes >= 1073741824) {
        tamaño = (double) bytes / 1073741824;
        unidad = "GB";
    }
    else if (bytes >= 1048576) {
        tamaño = (double) bytes / 1048576;
        unidad = "MB";
    }
    else if (bytes >= 1024) {
        tamaño = (double) bytes / 1024;
        unidad = "KB";
    }
    else
    {
        tamaño = (double) bytes;
        unidad = "B";
    }

    char temp[BUFFER_SIZE];
    sprintf(temp, "%.0f", tamaño);
    int tsize = atoi(temp);

    if (tsize - tamaño == 0)
        sprintf (buffer, "%.0f %s", tamaño, unidad);
    else
        sprintf (buffer, "%.2f %s", tamaño, unidad);
    return buffer;
}


void send_response(int client_socket, char* response, int response_length) {
    char headers[BUFFER_SIZE];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", response_length);
    send(client_socket, headers, strlen(headers), MSG_NOSIGNAL);
    send(client_socket, response, response_length, MSG_NOSIGNAL);
}

void send_directory_listing(int client_socket, char* directory_path, int fieldOrder, int Asc) {
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
                {
                    if(fieldOrder == NAME)
                        insertName(folders, entry, st, Asc);
                    else if(fieldOrder == SIZE)
                        insertName(folders, entry, st, TRUE);
                    else if(fieldOrder == DATE)
                        insertDate(folders, entry, st, Asc);
                }
            }
            else
            {
                if(files == NULL)
                    files = create_list(entry, st);
                else
                {
                    if(fieldOrder == NAME)
                        insertName(files, entry, st, Asc);
                    else if(fieldOrder == SIZE)
                        insertSize(files, entry, st, Asc);
                    else if(fieldOrder == DATE)
                        insertDate(files, entry, st, Asc);
                }
            }
        }
        else
        {
            if(files == NULL)
                files = create_list(entry, st);
            else
            {
                if(fieldOrder == NAME)
                    insertName(files, entry, st, Asc);
                else if(fieldOrder == SIZE)
                    insertSize(files, entry, st, Asc);
                else if(fieldOrder == DATE)
                    insertDate(files, entry, st, Asc);
            }
        }
    }

    char response[BUFFER_SIZE];
    int response_length = 0;

    if(Asc == FALSE)
        response_length += sprintf(response + response_length, "<html><head> <meta charset=\"UTF-8\"> <style> .bi-folder-fill-yellow{display:inline-block;width:1em;height:1em;vertical-align:-0.125em;background-image:url(\"data:image/svg+xml,%%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' fill='%%23f7d51d'%%3E%%3Cpath d='M14.5 6h-5.8l-1-2H4.5c-1.1 0-2 .9-2 2v8c0 1.1.9 2 2 2h9c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zM7 12H5v-2h2v2zm0-3H5V7h2v2z'/%%3E%%3C/svg%%3E\");background-repeat:no-repeat;background-size:1em} i { margin-right: 5px; } table{width:80%%;max-width:1000px;margin-left:auto;margin-right:auto;border-collapse:collapse}thead{background-color:#333;color:#fff}th{padding:12px}tbody tr:nth-child(even){background-color:#f2f2f2}td{padding:10px}td:first-child{text-align:left}td:last-child{text-align:center}td:nth-child(2){text-align:right;padding-right:20px} #sinh { user-select: none; text-decoration: none; color: black; } </style><title>Directorio %s</title></head><body><table><tr><th><a id=\"sinh\" href=\"?order=nameAsc\">Name</a></th><th><a id=\"sinh\" href=\"?order=sizeAsc\">Size</a></th><th><a id=\"sinh\" href=\"?order=dateAsc\">Last Modification Date</a></th></tr>", directory_path);

    else if(fieldOrder == NAME)
        response_length += sprintf(response + response_length, "<html><head> <meta charset=\"UTF-8\"> <style> .bi-folder-fill-yellow{display:inline-block;width:1em;height:1em;vertical-align:-0.125em;background-image:url(\"data:image/svg+xml,%%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' fill='%%23f7d51d'%%3E%%3Cpath d='M14.5 6h-5.8l-1-2H4.5c-1.1 0-2 .9-2 2v8c0 1.1.9 2 2 2h9c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zM7 12H5v-2h2v2zm0-3H5V7h2v2z'/%%3E%%3C/svg%%3E\");background-repeat:no-repeat;background-size:1em} i { margin-right: 5px; } table{width:80%%;max-width:1000px;margin-left:auto;margin-right:auto;border-collapse:collapse}thead{background-color:#333;color:#fff}th{padding:12px}tbody tr:nth-child(even){background-color:#f2f2f2}td{padding:10px}td:first-child{text-align:left}td:last-child{text-align:center}td:nth-child(2){text-align:right;padding-right:20px} #sinh { user-select: none; text-decoration: none; color: black; } </style><title>Directorio %s</title></head><body><table><tr><th><a id=\"sinh\" href=\"?order=nameDesc\">Name</a></th><th><a id=\"sinh\" href=\"?order=sizeAsc\">Size</a></th><th><a id=\"sinh\" href=\"?order=dateAsc\">Last Modification Date</a></th></tr>", directory_path);

    else if(fieldOrder == SIZE)
        response_length += sprintf(response + response_length, "<html><head> <meta charset=\"UTF-8\"> <style> .bi-folder-fill-yellow{display:inline-block;width:1em;height:1em;vertical-align:-0.125em;background-image:url(\"data:image/svg+xml,%%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' fill='%%23f7d51d'%%3E%%3Cpath d='M14.5 6h-5.8l-1-2H4.5c-1.1 0-2 .9-2 2v8c0 1.1.9 2 2 2h9c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zM7 12H5v-2h2v2zm0-3H5V7h2v2z'/%%3E%%3C/svg%%3E\");background-repeat:no-repeat;background-size:1em} i { margin-right: 5px; } table{width:80%%;max-width:1000px;margin-left:auto;margin-right:auto;border-collapse:collapse}thead{background-color:#333;color:#fff}th{padding:12px}tbody tr:nth-child(even){background-color:#f2f2f2}td{padding:10px}td:first-child{text-align:left}td:last-child{text-align:center}td:nth-child(2){text-align:right;padding-right:20px} #sinh { user-select: none; text-decoration: none; color: black; } </style><title>Directorio %s</title></head><body><table><tr><th><a id=\"sinh\" href=\"?order=nameAsc\">Name</a></th><th><a id=\"sinh\" href=\"?order=sizeDesc\">Size</a></th><th><a id=\"sinh\" href=\"?order=dateAsc\">Last Modification Date</a></th></tr>", directory_path);

    else if(fieldOrder == DATE)
        response_length += sprintf(response + response_length, "<html><head> <meta charset=\"UTF-8\"> <style> .bi-folder-fill-yellow{display:inline-block;width:1em;height:1em;vertical-align:-0.125em;background-image:url(\"data:image/svg+xml,%%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' fill='%%23f7d51d'%%3E%%3Cpath d='M14.5 6h-5.8l-1-2H4.5c-1.1 0-2 .9-2 2v8c0 1.1.9 2 2 2h9c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zM7 12H5v-2h2v2zm0-3H5V7h2v2z'/%%3E%%3C/svg%%3E\");background-repeat:no-repeat;background-size:1em} i { margin-right: 5px; } table{width:80%%;max-width:1000px;margin-left:auto;margin-right:auto;border-collapse:collapse}thead{background-color:#333;color:#fff}th{padding:12px}tbody tr:nth-child(even){background-color:#f2f2f2}td{padding:10px}td:first-child{text-align:left}td:last-child{text-align:center}td:nth-child(2){text-align:right;padding-right:20px} #sinh { user-select: none; text-decoration: none; color: black; } </style><title>Directorio %s</title></head><body><table><tr><th><a id=\"sinh\" href=\"?order=nameAsc\">Name</a></th><th><a id=\"sinh\" href=\"?order=sizeAsc\">Size</a></th><th><a id=\"sinh\" href=\"?order=dateDesc\">Last Modification Date</a></th></tr>", directory_path);

    if(fieldOrder != NAME)
    {
        struct nodo* file = files;
        while(file != NULL)
        {
            struct tm* t = localtime(&(file->st).st_mtime); 
            char buffer [20];
            strftime(buffer, 20, "%d-%m-%Y %H:%M", t);
            char *size = formatearTamaño((file->st).st_size);
            response_length += sprintf(response + response_length, "<tr><td><a href=\"javascript:void(0)\" onclick=\"clickFile('%s')\">%s</a></td><td style=\"white-space: nowrap;\">%s</td><td>%s</td></tr>", file->entry->d_name, file->entry->d_name, size, buffer);
            file = file->sig;
            free(size);
        }
        freeNodo(files);

        struct nodo* folder = folders;
        while(folder != NULL)
        {
            struct tm* t = localtime(&(folder->st).st_mtime);
            char buffer [20];
            strftime(buffer, 20, "%d-%m-%Y %H:%M", t); 
            response_length += sprintf(response + response_length, "<tr><td><i class=\"bi bi-folder-fill-yellow\"></i><a href=\"javascript:void(0)\" onclick=\"window.location.href = encodeURIComponent('%s') + '/'\">%s</a></td><td></td><td>%s</td></tr>", folder->entry->d_name, folder->entry->d_name, buffer);
            folder = folder->sig;
        }
        freeNodo(folders);
    }

    else
    {
        struct nodo* folder = folders;
        while(folder != NULL)
        {
            struct tm* t = localtime(&(folder->st).st_mtime);
            char buffer [20];
            strftime(buffer, 20, "%d-%m-%Y %H:%M", t); 
            response_length += sprintf(response + response_length, "<tr><td><i class=\"bi bi-folder-fill-yellow\"></i><a href=\"javascript:void(0)\" onclick=\"window.location.href = encodeURIComponent('%s') + '/'\">%s</a></td><td></td><td>%s</td></tr>", folder->entry->d_name, folder->entry->d_name, buffer);
            folder = folder->sig;
        }
        freeNodo(folders);

        struct nodo* file = files;
        while(file != NULL)
        {
            struct tm* t = localtime(&(file->st).st_mtime); 
            char buffer [20];
            strftime(buffer, 20, "%d-%m-%Y %H:%M", t);
            char *size = formatearTamaño((file->st).st_size);
            response_length += sprintf(response + response_length, "<tr><td><a href=\"javascript:void(0)\" onclick=\"clickFile('%s')\">%s</a></td><td style=\"white-space: nowrap;\">%s</td><td>%s</td></tr>", file->entry->d_name, file->entry->d_name, size, buffer);
            file = file->sig;
            free(size);
        }
        freeNodo(files);
    }

    response_length += sprintf(response + response_length, "<script> function clickFile(fileName) { var link = document.createElement(\"a\"); link.href = encodeURIComponent(fileName) + '/'; link.download = fileName; link.click(); } </script>");
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
    send(client_socket, headers, strlen(headers), MSG_NOSIGNAL);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_descriptor, buffer, sizeof(buffer))) > 0) {
        if (send(client_socket, buffer, bytes_read, MSG_NOSIGNAL) == -1) {
            break;
        }
    }

    //sendfile(client_socket, file_descriptor, 0, buf.st_size);
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
    char* newpath = url_to_path(path);

    int fieldOrder = NAME;
    int Asc = TRUE;

    char *again = strstr(newpath, "?");

    if(again != NULL)
    {
        if(strcmp(again + 1, "order=nameAsc") == 0)
        {
            fieldOrder = NAME;
            Asc = TRUE;
        }
        else if(strcmp(again + 1, "order=nameDesc") == 0)
        {
            fieldOrder = NAME;
            Asc = FALSE;
        }
        else if(strcmp(again + 1, "order=sizeAsc") == 0)
        {
            fieldOrder = SIZE;
            Asc = TRUE;
        }
        else if(strcmp(again + 1, "order=sizeDesc") == 0)
        {
            fieldOrder = SIZE;
            Asc = FALSE;
        }
        else if(strcmp(again + 1, "order=dateAsc") == 0)
        {
            fieldOrder = DATE;
            Asc = TRUE;
        }
        else if(strcmp(again + 1, "order=dateDesc") == 0)
        {
            fieldOrder = DATE;
            Asc = FALSE;
        }

        again[0] = '\0';
    }

    int len1 = strlen(init_path);
    int len2 = strlen(newpath);
    char newPath[len1 + len2];
    sprintf(newPath, "%s%s", init_path, newpath);
    newPath[len1 + len2] = '\0';
    struct stat st;
    if (stat(newPath, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            send_directory_listing(client_socket, newPath, fieldOrder, Asc);
        }
        else {
            send_file(client_socket, newPath);
        }
    }
    else {
        send_file(client_socket, newPath);
    }
    free(newpath);
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

void sigint_handler (int sig) {
    close(server_socket);
    printf("\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    signal(SIGINT, sigint_handler);
    
    int PORT = 8080;
    init_path = "/home";

    if(argc >= 3)
    {
        PORT = atoi(argv[1]);
        init_path = argv[2];
    }

    int client_socket;
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
    
    return 0;
}