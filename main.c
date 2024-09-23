#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdbool.h>

void handle_err_404(int client_fd) {
    const char* not_found_response = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: 13\r\n"
                    "\r\n"
                    "404 Not Found";
    send(client_fd, not_found_response, strlen(not_found_response), 0);
}

void handle_err_500(int client_fd) {
    const char* not_found_response = 
                    "HTTP/1.1 500 Internal Server Error\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: 25\r\n"
                    "\r\n"
                    "500 Internal Server Error";
    send(client_fd, not_found_response, strlen(not_found_response), 0);
}


int handle_get_request(char *buffer, int client_fd) {
    char* f = buffer + 4;
    char* end_of_path = strchr(f, ' ');
    if (end_of_path != NULL) {
        *end_of_path = 0;
    } else {
        close(client_fd);
        return 1;
    }

    char filepath[256] = "./";
    strcat(filepath, f);
    int file_fd = open(filepath, O_RDONLY);

    if (file_fd < 0) {
        handle_err_404(client_fd);
    } else {
        char response_header[256];
        sprintf(response_header, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "\r\n");
        send(client_fd, response_header, strlen(response_header), 0);

        off_t offset = 0;
        struct stat stat_buf;
        fstat(file_fd, &stat_buf);
        sendfile(client_fd, file_fd, &offset, stat_buf.st_size);
        close(file_fd);
    }
    return 0;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Server is running on port 8080\n");

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[1024] = {0};
        recv(client_fd, buffer, 1024, 0);
        printf("Request: %s\n", buffer);

        if (strncmp(buffer, "GET ", 4) == 0) {
            int res = handle_get_request(buffer, client_fd);

            if(res == 1) {
                handle_err_500(client_fd);
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
