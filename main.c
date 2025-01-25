#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

void handle_err_404(int client_fd) {
    const char *not_found_response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "404 Not Found";
    send(client_fd, not_found_response, strlen(not_found_response), 0);
}

void handle_err_400(int client_fd, const char* message) {
    char response[512];
    const char *header =
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %ld\r\n"
        "\r\n";
    
    size_t message_length = strlen(message);

    sprintf(response, header, message_length);
    strcat(response, message);

    send(client_fd, response, strlen(response), 0);
}

long get_file_size(const char *filename) {
    struct stat st;

    if (stat(filename, &st) == 0) {
        return st.st_size;
    }

    perror("Error getting file size");
    return -1;
}

void handle_get_request(char *buffer, int client_fd) {
    char *header;
    char *rest = buffer;
    char range_header[256] = {0};
    
    long start = -1, end = -1;

    // Parse Range header
    while ((header = strtok_r(rest, "\r\n", &rest))) {
        if (strncmp(header, "Range:", 6) == 0) {
            char *message = header + 6;
            while (*message == ' ') message++;
            strncpy(range_header, message, sizeof(range_header) - 1);
            range_header[sizeof(range_header) - 1] = '\0';
        }
    }

    if (strlen(range_header) == 0) {
        handle_err_404(client_fd);
        return;
    }

    long filesize = get_file_size("bunny.mp4");
    if (filesize < 0) {
        handle_err_400(client_fd, "Unable to retrieve file size");
        return;
    }

    // Handle range: bytes=0- or bytes=100- without an end byte
    if (strncmp(range_header, "bytes=", 6) == 0) {
        char *range_part = range_header + 6;
        
        char *dash_pos = strchr(range_part, '-');
        if (dash_pos != NULL) {
            *dash_pos = '\0';
            start = atol(range_part);
            if (*(dash_pos + 1) != '\0') {
                end = atol(dash_pos + 1);
            } else {
                end = start + 1048576 - 1; // If no end is provided, send 1MB
            }
        } else {
            start = atol(range_part);
            end = start + 1048576 - 1; // Default 1MB
        }

        // Ensure end is within bounds
        if (end >= filesize) {
            end = filesize - 1;
        }

        if (start >= filesize) {
            handle_err_400(client_fd, "Start range is beyond file size");
            return;
        }

        printf("Streaming range for bunny.mp4: start = %ld, end = %ld\n", start, end);
    }

    // TODO: Handle any file inside some video directory
    FILE *file = fopen("bunny.mp4", "rb");
    if (file == NULL) {
        handle_err_400(client_fd, "Unable to open file");
        return;
    }

    fseek(file, start, SEEK_SET);
    long content_length = end - start + 1;
    char response_header[512];

    sprintf(response_header,
            "HTTP/1.1 206 Partial Content\r\n"
            "Content-Type: video/mp4\r\n"
            "Content-Range: bytes %ld-%ld/%ld\r\n"
            "Content-Length: %ld\r\n"
            "\r\n",
            start, end, filesize, content_length);

    // Send the header (check for failure)
    ssize_t bytes_sent = send(client_fd, response_header, strlen(response_header), 0);
    if (bytes_sent < 0) {
        if (errno == EPIPE) {
            perror("Client disconnected");
            fclose(file);
            return;
        }
        perror("Send failed");
        fclose(file);
        return;
    }

    char buffer_video[1024];
    while (content_length > 0) {
        size_t bytes_to_read = content_length > sizeof(buffer_video) ? sizeof(buffer_video) : content_length;
        size_t bytes_read = fread(buffer_video, 1, bytes_to_read, file);
        if (bytes_read > 0) {
            bytes_sent = send(client_fd, buffer_video, bytes_read, 0);
            if (bytes_sent < 0) {
                if (errno == EPIPE) {
                    perror("Client disconnected");
                    break;
                }
                perror("Send failed");
                break;
            }
            content_length -= bytes_sent;
        } else {
            break;
        }
    }

    fclose(file);
}

int main() {
    // Avoid program exit on canceled request
    signal(SIGPIPE, SIG_IGN);

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

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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

        char buffer[4096] = {0};
        int received_bytes = 0;
        while (1) {
            int bytes = recv(client_fd, buffer + received_bytes, sizeof(buffer) - received_bytes - 1, 0);
            if (bytes <= 0) {
                perror("Recv failed");
                close(client_fd);
                break;
            }
            received_bytes += bytes;
            buffer[received_bytes] = '\0';

            if (strstr(buffer, "\r\n\r\n")) {
                handle_get_request(buffer, client_fd);
                break;
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
