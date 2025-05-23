// udp_tictactoe_client.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>

#define PORT 8080
#define MAX_BUFFER_SIZE 1024

// Function prototypes
void to_lowercase(char *str);
void trim_newline(char *str);

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[MAX_BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    char server_ip[INET_ADDRSTRLEN]; // Buffer for server IP address input

    // Prompt for the server's IPv4 address
    printf("Enter the server's IPv4 address (e.g., 192.168.1.1): ");
    fgets(server_ip, sizeof(server_ip), stdin);
    trim_newline(server_ip); // Remove newline character from input

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    
    // Use the user-provided IP address
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", server_ip);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    server_addr.sin_port = htons(PORT);

    // Send a connection message to the server
    strcpy(buffer, "Client connected");
    if (sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Failed to send connection message");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server at %s. Waiting for another player...\n", server_ip);

    while (1) {
        // Receive messages from the server
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
        if (n < 0) {
            perror("Failed to receive message");
            continue;
        }
        buffer[n] = '\0';
        printf("%s\n", buffer);

        // Check for play again prompt
        if (strstr(buffer, "Do you want to play again?") != NULL) {
            char response[10];
            printf("Enter your response (yes/no): ");
            scanf("%s", response);
            trim_newline(response);
            to_lowercase(response);

            // Validate input
            while (strcmp(response, "yes") != 0 && strcmp(response, "no") != 0) {
                printf("Invalid input. Please enter 'yes' or 'no': ");
                scanf("%s", response);
                trim_newline(response);
                to_lowercase(response);
            }

            // Send the response to the server
            if (sendto(sockfd, response, strlen(response), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                perror("Failed to send response");
                close(sockfd);
                exit(EXIT_FAILURE);
            }
            printf("Response sent: %s\n", response);
            continue;
        }

        // If prompted for a move
        if (strstr(buffer, "Your turn") != NULL) {
            int row, col;
            while (1) { // Loop until valid input is received
                printf("Enter your move (row and column): ");
                // Ensure proper input handling
                if (scanf("%d %d", &row, &col) != 2) {
                    printf("Invalid input format. Please enter two numbers separated by space.\n");
                    // Clear the input buffer
                    while (getchar() != '\n'); // Read and discard characters until the newline
                    continue; // Re-prompt for move
                }

                // Validate row and column
                if (row < 1 || row > 3 || col < 1 || col > 3) {
                    printf("Invalid move. Rows and columns must be between 1 and 3.\n");
                    continue; // Re-prompt for move
                }

                // Send the move to the server
                sprintf(buffer, "%d %d", row, col);
                if (sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                    perror("Failed to send move");
                    close(sockfd);
                    exit(EXIT_FAILURE);
                }
                printf("Move sent: %d %d\n", row, col);
                break; // Exit the loop after valid input
            }
            continue;
        }

        // Handle game result messages (win or draw) but do not exit
        if (strstr(buffer, "wins") != NULL || strstr(buffer, "draw") != NULL) {
            // These messages are already printed above; no action needed
            // The client should wait for the "Do you want to play again?" prompt
            continue;
        }

        // Handle game termination messages and exit
        if (strstr(buffer, "Closing connection") != NULL || strstr(buffer, "Game over") != NULL) {
            printf("Game concluded. Exiting.\n");
            break;
        }

        // Optionally, handle other server messages here
    }

    // Close the socket
    close(sockfd);
    return 0;
}

// Converts a string to lowercase
void to_lowercase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

// Removes the newline character from a string, if present
void trim_newline(char *str) {
    size_t len = strlen(str);
    if (len == 0) return;
    if (str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

