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
    char *first_line_end = strstr(buffer, "\n");
    if (first_line_end != NULL) {
        buffer = first_line_end + 1;
    }

    if(sizeof(buffer) > 8000) {
        char response_header[256];
        sprintf(response_header, 
                "HTTP/1.1 413 Entity Too Large\r\n"
                "Content-Type: text/html\r\n"
                "\r\n");
        send(client_fd, response_header, strlen(response_header), 0);

        close(client_fd);
        return 1;
    }

    char* header;
    char* rest = buffer;

    while ((header = strtok_r(rest, "\n", &rest))) {
        if (strcmp(header, "\r") == 0 || strcmp(header, "") == 0) {
            break;
        }
        // TODO: Handle extraction of video specific headers
        printf("%s\n", header);
    }

    char response_header[256];
    sprintf(response_header, 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n");
    send(client_fd, response_header, strlen(response_header), 0);

    close(client_fd);
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
