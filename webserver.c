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

#define PORT 8080
#define BUFFER_SIZE 10240

void send_response(int client_socket, char* response, int response_length) {
    char headers[BUFFER_SIZE];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", response_length);
    send(client_socket, headers, strlen(headers), 0);
    send(client_socket, response, response_length, 0);
}

void send_directory_listing(int client_socket, char* directory_path) {
    DIR* directory = opendir(directory_path);
    struct dirent* entry;
    char response[BUFFER_SIZE];
    int response_length = 0;

    response_length += sprintf(response + response_length, "<html><head><title>Directorio %s</title></head><body><table><tr><th>Name</th></tr>", directory_path);

    while ((entry = readdir(directory)) != NULL) {
        struct stat st;
        char name[BUFFER_SIZE];
        sprintf(name, "%s%s", directory_path, entry->d_name);
        if (stat(name, &st) == 0) {
            if (S_ISDIR(st.st_mode))
                response_length += sprintf(response + response_length, "<tr><td><a href=\"%s/\">%s</a></td></tr>", entry->d_name, entry->d_name);
            else
                response_length += sprintf(response + response_length, "<tr><td><a href=\"%s/\" download=\"%s\">%s</a></td></tr>", entry->d_name, entry->d_name, entry->d_name);
        }
        else {
            response_length += sprintf(response + response_length, "<tr><td><a href=\"%s/\" download=\"%s\">%s</a></td></tr>", entry->d_name, entry->d_name, entry->d_name);
        }
    }

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
    struct stat buf; // Crear una estructura stat
    fstat (file_descriptor, &buf);   

    char header [100];
    char* name = strrchr(file_path, '/');
    sprintf (header, "HTTP/1.1 200 OK\\r\\nContent-Length: %ld\\r\\nContent-Disposition: attachment; filename=%s\\r\\n\\r\\n", buf.st_size, name + 1);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = read(file_descriptor, buffer, BUFFER_SIZE)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }
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
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            send_directory_listing(client_socket, path);
        }
        else {
            send_file(client_socket, path);
        }
    }
    else {
        send_file(client_socket, path);
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

int main() {
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
    return 0;
}