//including the essentiual headers files required for this code to work
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

//defining buffefr size
#define BUFFER_SIZE 4096
void receive_tar_file(int server, const char* tarFileName) {
    uint32_t fileSize;
    // First, receive the size of the tar file
    int bytes_read = recv(server, &fileSize, sizeof(fileSize), 0);
    if (bytes_read != sizeof(fileSize)) {
        perror("Failed to receive file size");
        return;
    }
    
    fileSize = ntohl(fileSize); // Convert the network byte order to host byte order

    FILE *tarFile = fopen(tarFileName, "w");
    if (!tarFile) {
        perror("Failed to open file for writing");
        return;
    }

    // Then receive the tar file content
    char buffer[BUFFER_SIZE];
    while (fileSize > 0) {
        bytes_read = recv(server, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) {
            perror("recv failed");
            break;
        }
        fwrite(buffer, 1, bytes_read, tarFile);
        fileSize -= bytes_read;
    }
    fclose(tarFile);

    // Add logic to check if the file was received successfully
}

int main(int argc, char *argv[]) {
    char message[BUFFER_SIZE] = {0};
    int server, portNumber;
    struct sockaddr_in servAdd;  // server socket address

    if (argc != 3) {
        printf("Call model: %s <IP Address> <Port Number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if ((server = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    servAdd.sin_family = AF_INET;
    sscanf(argv[2], "%d", &portNumber);
    servAdd.sin_port = htons((uint16_t)portNumber);

    if (inet_pton(AF_INET, argv[1], &servAdd.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(server, (struct sockaddr *)&servAdd, sizeof(servAdd)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    char response1[BUFFER_SIZE] = {0};
    while (1) {
     
        printf("Enter command for server to run (Enter $ to quit):\n");
        fgets(message, BUFFER_SIZE, stdin);
        if (strncmp(message, "$", 1) == 0) {
            printf("Closing socket and exiting!\n");
            write(server, message, strlen(message) + 1);
            close(server);
            exit(EXIT_SUCCESS);
        }

        // write to server
        if (write(server, message, strlen(message) + 1) < 0) {
            perror("Write failed");
            close(server);
            exit(EXIT_FAILURE);
        }

        if (strncmp(message, "w24fz",5) == 0) {
           if (write(server, message, strlen(message)) < 0) {
                perror("Write failed");
                close(server);
                exit(EXIT_FAILURE);
            }

            // Wait for server response to either start receiving file or get a message
            char response[BUFFER_SIZE] = {0};
            if (recv(server, response, sizeof(response), 0) > 0) {
                if (strcmp(response, "No file found\n") == 0) {
                    printf("Server response: %s", response);
                } else if (strncmp(response, "Starting file transfer", 22) == 0) {
                    receive_tar_file(server, "temp.tar.gz"); 
                } else {
                    printf("Unexpected server response: %s", response);
                }
            }
        }
        // read from server
        char buffer[BUFFER_SIZE];
        ssize_t bytesReceived;
        int totalBytesReceived = 0;
        while ((bytesReceived = recv(server, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0';  // Null-terminate the string
            totalBytesReceived += bytesReceived;
            printf("%s", buffer);
            if (bytesReceived < BUFFER_SIZE - 1) {
                // If received less than buffer size, assume end of data and break
                break;
            }
        }

        if (bytesReceived < 0) {
            perror("Receive failed");
            close(server);
            exit(EXIT_FAILURE);
        }

        if (totalBytesReceived == 0) {
            printf("No data received from server.\n");
        }
    }

    return 0;
}
