// udp_tictactoe_server.c

#define _POSIX_C_SOURCE 200112L // Define POSIX standard before includes

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>   // For struct timeval
#include <ifaddrs.h>    // For getifaddrs
#include <netdb.h>      // For getnameinfo
#include <strings.h>    // For strncasecmp
#define NI_MAXHOST 1024

#define PORT 8080
#define MAX_CLIENTS 2
#define BOARD_SIZE 3
#define MAX_BUFFER_SIZE 1024
#define TIMEOUT_SEC 30  // Timeout in seconds for client responses

typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    int currentPlayer; // 0 for Player 1, 1 for Player 2
} GameState;

// Function prototypes
void initBoard(GameState *game);
void printBoard(GameState *game);
int checkWin(GameState *game);
void broadcastBoard(GameState *game, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd);
int receiveResponse(int sockfd, struct sockaddr_in *expected_client, socklen_t addr_len, char *response);
void sendMessage(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, const char *message);
void setSocketTimeout(int sockfd, int sec);
void print_ipv4_addresses();

// Initialize the game board
void initBoard(GameState *game) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            game->board[i][j] = ' ';
        }
    }
    game->currentPlayer = 0; // Player 1 starts
}

// Print the game board to the server console
void printBoard(GameState *game) {
    printf("\nCurrent Board:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf(" %c ", game->board[i][j]);
            if (j < BOARD_SIZE - 1) printf("|");
        }
        printf("\n");
        if (i < BOARD_SIZE - 1) {
            printf("---|---|---\n");
        }
    }
    printf("\n");
}

// Check for a win or a draw
int checkWin(GameState *game) {
    char player = (game->currentPlayer == 0) ? 'X' : 'O';

    // Check rows and columns
    for (int i = 0; i < BOARD_SIZE; i++) {
        if ((game->board[i][0] == player && game->board[i][1] == player && game->board[i][2] == player) ||
            (game->board[0][i] == player && game->board[1][i] == player && game->board[2][i] == player)) {
            return 1; // Win
        }
    }

    // Check diagonals
    if ((game->board[0][0] == player && game->board[1][1] == player && game->board[2][2] == player) ||
        (game->board[0][2] == player && game->board[1][1] == player && game->board[2][0] == player)) {
        return 1; // Win
    }

    // Check for draw
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game->board[i][j] == ' ') return 0; // Not finished
        }
    }

    return -1; // Draw
}

// Broadcast the current board to both clients
void broadcastBoard(GameState *game, struct sockaddr_in *client_addr, socklen_t addr_len, int sockfd) {
    char buffer[MAX_BUFFER_SIZE];
    strcpy(buffer, "Current board:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            char cell[4];
            snprintf(cell, sizeof(cell), " %c ", game->board[i][j]);
            strcat(buffer, cell);
            if (j < BOARD_SIZE - 1) strcat(buffer, "|");
        }
        strcat(buffer, "\n");
        if (i < BOARD_SIZE - 1) {
            strcat(buffer, "---|---|---\n");
        }
    }
    strcat(buffer, "\n");

    // Send the board to both clients
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *)&client_addr[i], addr_len) < 0) {
            perror("Failed to send board");
        } else {
            printf("Sent board to Client %d (%s:%d)\n", i + 1, 
                   inet_ntoa(client_addr[i].sin_addr), ntohs(client_addr[i].sin_port));
        }
    }
}

// Receive a response from a specific client, ensuring the response is from the expected client
int receiveResponse(int sockfd, struct sockaddr_in *expected_client, socklen_t addr_len, char *response) {
    while (1) {
        struct sockaddr_in src_addr;
        socklen_t src_addr_len = sizeof(src_addr);
        int n = recvfrom(sockfd, response, MAX_BUFFER_SIZE - 1, 0, (struct sockaddr *)&src_addr, &src_addr_len);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Timeout occurred
                printf("Timeout: Client (%s:%d) did not respond.\n",
                       inet_ntoa(expected_client->sin_addr), ntohs(expected_client->sin_port));
                return -1;
            } else {
                perror("Failed to receive response");
                return -1;
            }
        }
        response[n] = '\0';

        // Verify that the response is from the expected client
        if (src_addr.sin_addr.s_addr == expected_client->sin_addr.s_addr &&
            src_addr.sin_port == expected_client->sin_port) {
            printf("Received response from expected client (%s:%d): %s\n",
                   inet_ntoa(expected_client->sin_addr), ntohs(expected_client->sin_port), response);
            return 0;
        } else {
            // Unexpected client, ignore the message
            printf("Received response from unknown client (%s:%d): %s\n",
                   inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), response);
            continue;
        }
    }
}

// Send a message to a specific client
void sendMessage(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, const char *message) {
    if (sendto(sockfd, message, strlen(message), 0, (const struct sockaddr *)client_addr, addr_len) < 0) {
        perror("Failed to send message");
    } else {
        printf("Sent message to client (%s:%d): %s\n",
               inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port), message);
    }
}

// Set receive timeout for the socket
void setSocketTimeout(int sockfd, int sec) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
        perror("Error setting socket timeout");
        exit(EXIT_FAILURE);
    }
}

// Function to print all IPv4 addresses of the server
void print_ipv4_addresses() {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    char host[NI_MAXHOST];

    printf("Server IPv4 Addresses:\n");

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Iterate through linked list
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family == AF_INET) { // IPv4
            // Get the address
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST,
                                NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                fprintf(stderr, "getnameinfo() failed: %s\n", gai_strerror(s));
                continue;
            }
            printf("  %s: %s\n", ifa->ifa_name, host);
        }
    }

    freeifaddrs(ifaddr);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr[MAX_CLIENTS];
    socklen_t addr_len = sizeof(struct sockaddr_in);
    GameState game;
    char buffer[MAX_BUFFER_SIZE];
    int clientsConnected = 0;

    // Print server's IPv4 addresses
    print_ipv4_addresses();

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket receive timeout
    setSocketTimeout(sockfd, TIMEOUT_SEC);

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running and waiting for %d clients...\n", MAX_CLIENTS);

    // Wait for two clients to connect
    while (clientsConnected < MAX_CLIENTS) {
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&client_addr[clientsConnected], &addr_len);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                printf("Waiting for clients... (Timeout reached)\n");
                continue;
            } else {
                perror("Failed to receive client message");
                continue;
            }
        }
        buffer[n] = '\0';
        printf("Client %d connected: %s from %s:%d\n", 
               clientsConnected + 1, buffer, 
               inet_ntoa(client_addr[clientsConnected].sin_addr), 
               ntohs(client_addr[clientsConnected].sin_port));
        clientsConnected++;
    }

    // Main loop to handle multiple games
    int serverActive = 1;
    while (serverActive) {
        // Initialize game state
        initBoard(&game);
        printBoard(&game);
        broadcastBoard(&game, client_addr, addr_len, sockfd);

        int gameActive = 1;
        while (gameActive) {
            int currentPlayer = game.currentPlayer;
            char playerSymbol = (currentPlayer == 0) ? 'X' : 'O';

            // Inform current player to make a move
            snprintf(buffer, sizeof(buffer), "Your turn (%c). Enter row and column (1-3): ", playerSymbol);
            sendMessage(sockfd, &client_addr[currentPlayer], addr_len, buffer);

            // Wait for move with timeout
            if (receiveResponse(sockfd, &client_addr[currentPlayer], addr_len, buffer) < 0) {
                // Client did not respond in time
                snprintf(buffer, sizeof(buffer), "Player %d has disconnected or did not respond. Ending game.\n", currentPlayer + 1);
                sendMessage(sockfd, &client_addr[0], addr_len, buffer);
                sendMessage(sockfd, &client_addr[1], addr_len, buffer);
                gameActive = 0;
                break;
            }

            int row, col;
            if (sscanf(buffer, "%d %d", &row, &col) != 2) {
                snprintf(buffer, sizeof(buffer), "Invalid input format. Please enter two numbers separated by space.\n");
                sendMessage(sockfd, &client_addr[currentPlayer], addr_len, buffer);
                continue;
            }

            // Convert to 0-based index for board access
            row -= 1;
            col -= 1;

            // Validate move
            if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE || game.board[row][col] != ' ') {
                snprintf(buffer, sizeof(buffer), "Invalid move. Try again.\n");
                sendMessage(sockfd, &client_addr[currentPlayer], addr_len, buffer);
                continue;
            }

            // Update board
            game.board[row][col] = playerSymbol;
            printBoard(&game);

            // Broadcast the updated board to both clients
            broadcastBoard(&game, client_addr, addr_len, sockfd);

            // Check for win or draw
            int result = checkWin(&game);
            if (result == 1) {
                snprintf(buffer, sizeof(buffer), "Player %d (%c) wins!\n", currentPlayer + 1, playerSymbol);
                sendMessage(sockfd, &client_addr[0], addr_len, buffer);
                sendMessage(sockfd, &client_addr[1], addr_len, buffer);
                gameActive = 0;
            } else if (result == -1) {
                snprintf(buffer, sizeof(buffer), "It's a draw!\n");
                sendMessage(sockfd, &client_addr[0], addr_len, buffer);
                sendMessage(sockfd, &client_addr[1], addr_len, buffer);
                gameActive = 0;
            }

            // Switch players if game is still active
            if (gameActive) {
                game.currentPlayer = 1 - currentPlayer;
            }
        }

        // After the game ends, ask both players if they want to play again
        snprintf(buffer, sizeof(buffer), "Do you want to play again? (yes/no): ");
        sendMessage(sockfd, &client_addr[0], addr_len, buffer);
        sendMessage(sockfd, &client_addr[1], addr_len, buffer);

        char response1[10], response2[10];
        // Receive responses from both clients with timeout
        if (receiveResponse(sockfd, &client_addr[0], addr_len, response1) < 0) {
            printf("Failed to receive response from Player 1.\n");
            snprintf(buffer, sizeof(buffer), "Player 1 did not respond. Ending session.\n");
            sendMessage(sockfd, &client_addr[1], addr_len, buffer);
            serverActive = 0;
            break;
        }
        if (receiveResponse(sockfd, &client_addr[1], addr_len, response2) < 0) {
            printf("Failed to receive response from Player 2.\n");
            snprintf(buffer, sizeof(buffer), "Player 2 did not respond. Ending session.\n");
            sendMessage(sockfd, &client_addr[0], addr_len, buffer);
            serverActive = 0;
            break;
        }

        // Convert responses to lowercase for comparison
        for (int i = 0; response1[i]; i++) response1[i] = tolower(response1[i]);
        for (int i = 0; response2[i]; i++) response2[i] = tolower(response2[i]);

        // Determine the next action based on responses
        if (strcmp(response1, "yes") == 0 && strcmp(response2, "yes") == 0) {
            // Both players want to play again
            printf("Both players agreed to play again. Starting a new game.\n");
            continue; // Start a new game
        } else if (strcmp(response1, "no") == 0 && strcmp(response2, "no") == 0) {
            // Both players do not want to play again
            snprintf(buffer, sizeof(buffer), "Game over. Thank you for playing!\n");
            sendMessage(sockfd, &client_addr[0], addr_len, buffer);
            sendMessage(sockfd, &client_addr[1], addr_len, buffer);
            printf("Both players chose not to play again. Shutting down server.\n");
            serverActive = 0;
        } else {
            // One player wants to continue, the other does not
            if (strcmp(response1, "yes") == 0 && strcmp(response2, "no") == 0) {
                snprintf(buffer, sizeof(buffer), "Player 2 does not want to continue. Closing connection.\n");
                sendMessage(sockfd, &client_addr[0], addr_len, buffer);
                sendMessage(sockfd, &client_addr[1], addr_len, buffer);
                printf("Player 1 wants to continue, but Player 2 does not. Shutting down server.\n");
            } else if (strcmp(response1, "no") == 0 && strcmp(response2, "yes") == 0) {
                snprintf(buffer, sizeof(buffer), "Player 1 does not want to continue. Closing connection.\n");
                sendMessage(sockfd, &client_addr[0], addr_len, buffer);
                sendMessage(sockfd, &client_addr[1], addr_len, buffer);
                printf("Player 2 wants to continue, but Player 1 does not. Shutting down server.\n");
            } else {
                // Handle unexpected responses
                snprintf(buffer, sizeof(buffer), "Invalid responses received. Closing connection.\n");
                sendMessage(sockfd, &client_addr[0], addr_len, buffer);
                sendMessage(sockfd, &client_addr[1], addr_len, buffer);
                printf("Invalid responses. Shutting down server.\n");
            }
            serverActive = 0;
        }
    }

    // Close the socket
    close(sockfd);
    return 0;
}

