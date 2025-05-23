#define _POSIX_C_SOURCE 200112L 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <strings.h>     
#include <errno.h>        
#define NI_MAXHOST 1024
#define PORT 12345
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    char symbol;
    int player_number;
    int wants_replay;
} Player;

char board[3][3];
Player players[2];
int current_player = 0; // 0 for Player 1, 1 for Player 2
int game_over = 0;
int clients_connected = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t replay_cond = PTHREAD_COND_INITIALIZER;

// Initialize the game board
void initialize_board() {
    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++)
            board[i][j] = ' ';
}

// Convert the board to a string
void get_board_str(char *buffer) {
    snprintf(buffer, BUFFER_SIZE,
            "\nCurrent Board:\n"
            "  1 2 3\n"
            "1 %c|%c|%c\n"
            "  -----\n"
            "2 %c|%c|%c\n"
            "  -----\n"
            "3 %c|%c|%c\n",
            board[0][0], board[0][1], board[0][2],
            board[1][0], board[1][1], board[1][2],
            board[2][0], board[2][1], board[2][2]);
}

// Send message to a specific player
int send_message(int socket, const char *message) {
    ssize_t total_sent = 0;
    ssize_t message_len = strlen(message);
    while (total_sent < message_len) {
        ssize_t sent = send(socket, message + total_sent, message_len - total_sent, 0);
        if (sent < 0) {
            perror("Send failed");
            return -1;
        }
        total_sent += sent;
    }
    return 0;
}

// Broadcast message to both players
void broadcast_message(const char *message) {
    for(int i = 0; i < 2; i++) {
        if(send_message(players[i].socket, message) < 0) {
            // Optionally handle send failure
            fprintf(stderr, "Failed to send message to Player %d\n", players[i].player_number);
        }
    }
}

// Check for a win or draw
int check_game_over(char symbol) {
    // Check rows and columns
    for(int i = 0; i < 3; i++) {
        if((board[i][0] == symbol && board[i][1] == symbol && board[i][2] == symbol) ||
           (board[0][i] == symbol && board[1][i] == symbol && board[2][i] == symbol))
            return 1;
    }
    // Check diagonals
    if((board[0][0] == symbol && board[1][1] == symbol && board[2][2] == symbol) ||
       (board[0][2] == symbol && board[1][1] == symbol && board[2][0] == symbol))
        return 1;
    // Check for draw
    int empty = 0;
    for(int i = 0; i < 3 && !empty; i++)
        for(int j = 0; j < 3 && !empty; j++)
            if(board[i][j] == ' ')
                empty = 1;
    if(!empty)
        return 2; // Draw
    return 0; // Game continues
}

// Handle replay responses
void handle_replay(int player_idx, const char *message) {
    if(strncasecmp(message, "yes", 3) == 0) { // Fixed: strncasecmp is declared
        players[player_idx].wants_replay = 1;
    } else {
        players[player_idx].wants_replay = 0;
    }
}

// Handle communication with a player
void *player_handler(void *arg) {
    Player *player = (Player *)arg;
    char buffer[BUFFER_SIZE];
    int opponent = (player->player_number == 1) ? 1 : 0;

    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes = recv(player->socket, buffer, BUFFER_SIZE - 1, 0);
        if(bytes <= 0) {
            if(bytes == 0){
                printf("Player %d disconnected gracefully.\n", player->player_number);
            }
            else{
                perror("recv failed");
            }
            close(player->socket);
            // Inform the opponent
            char msg[BUFFER_SIZE];
            snprintf(msg, BUFFER_SIZE, "Player %d has disconnected. Game over.\n", player->player_number);
            send_message(players[opponent].socket, msg);
            exit(1);
        }
        buffer[bytes] = '\0';

        pthread_mutex_lock(&lock);

        if(game_over == 0 && player->player_number == (current_player + 1)) {
            // Process move
            int row, col;
            if(sscanf(buffer, "%d %d", &row, &col) != 2) {
                send_message(player->socket, "Invalid format. Use: row col (e.g., 1 1)\n");
                pthread_mutex_unlock(&lock);
                continue;
            }
            row--; col--; // Convert to 0-based index
            if(row < 0 || row >=3 || col < 0 || col >=3 || board[row][col] != ' ') {
                send_message(player->socket, "Invalid move. Try again.\n");
                pthread_mutex_unlock(&lock);
                continue;
            }
            board[row][col] = player->symbol;

            // Broadcast updated board
            char board_str[BUFFER_SIZE];
            get_board_str(board_str);
            broadcast_message(board_str);

            // Check game status
            int status = check_game_over(player->symbol);
            if(status == 1) {
                char win_msg[BUFFER_SIZE];
                snprintf(win_msg, BUFFER_SIZE, "Player %d Wins!\n", player->player_number);
                broadcast_message(win_msg);
                game_over = 1;
                // Prompt for replay
                broadcast_message("Do you want to play again? (yes/no):\n");
                // Signal to main thread that game is over and waiting for replay
                pthread_cond_signal(&replay_cond);
            }
            else if(status == 2) {
                broadcast_message("It's a Draw!\n");
                game_over = 1;
                // Prompt for replay
                broadcast_message("Do you want to play again? (yes/no):\n");
                // Signal to main thread that game is over and waiting for replay
                pthread_cond_signal(&replay_cond);
            }
            else {
                // Switch turn
                current_player = 1 - current_player;
                char turn_msg[BUFFER_SIZE];
                snprintf(turn_msg, BUFFER_SIZE, "Player %d's turn.\n", current_player + 1);
                broadcast_message(turn_msg);
            }
        }
        else if(game_over == 1) {
            // Handle replay decision
            handle_replay(player->player_number - 1, buffer);
            // Check if both players have responded
            int both_responded = 1;
            for(int i = 0; i < 2; i++) {
                if(players[i].wants_replay == -1) {
                    both_responded = 0;
                    break;
                }
            }
            if(both_responded){
                // Both have responded
                pthread_cond_signal(&replay_cond);
            }
        }
        else {
            // Player attempted to make a move out of turn
            char error_msg[BUFFER_SIZE];
            snprintf(error_msg, BUFFER_SIZE, "It's not your turn. Please wait for Player %d to make a move.\n", (current_player + 1));
            send_message(player->socket, error_msg);
        }


        pthread_mutex_unlock(&lock);
    }
    return NULL;
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
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    initialize_board();

    // Print IPv4 addresses
    print_ipv4_addresses();

    // Create socket file descriptor
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Attach socket to the port
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Define server address
    memset(&address, 0, sizeof(address)); // Ensure the structure is zeroed
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if(listen(server_fd, 2) < 0) {
        perror("Listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Server listening on port %d\n", PORT);

    // Accept two clients
    for(int i = 0; i < 2; i++) {
        if((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("Accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        players[i].socket = new_socket;
        players[i].symbol = (i == 0) ? 'X' : 'O';
        players[i].player_number = i + 1;
        players[i].wants_replay = -1; // -1 indicates no response yet
        clients_connected++;
        printf("Player %d connected from %s:%d.\n", players[i].player_number,
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        // Send welcome message
        char welcome_msg[BUFFER_SIZE];
        snprintf(welcome_msg, BUFFER_SIZE, "Welcome Player %d! You are '%c'\n", players[i].player_number, players[i].symbol);
        send_message(new_socket, welcome_msg);
    }

    // Broadcast initial board
    char board_str[BUFFER_SIZE];
    get_board_str(board_str);
    broadcast_message(board_str);

    // Notify the first player to start
    char turn_msg[BUFFER_SIZE];
    snprintf(turn_msg, BUFFER_SIZE, "Player %d's turn.\n", current_player + 1);
    broadcast_message(turn_msg);

    // Create threads for each player
    pthread_t threads[2];
    for(int i = 0; i < 2; i++) {
        if(pthread_create(&threads[i], NULL, player_handler, (void *)&players[i]) != 0) {
            perror("pthread_create");
            close(players[i].socket);
            // Close other sockets if already opened
            for(int j = 0; j < i; j++) {
                close(players[j].socket);
            }
            close(server_fd);
            exit(EXIT_FAILURE);
        }
    }

    // Wait for both players to respond to replay prompt
    pthread_mutex_lock(&lock);
    while(1) {
        // Wait until game_over is set
        while(!(game_over == 1)) {
            pthread_cond_wait(&replay_cond, &lock);
        }

        // Now, game_over == 1 and at least one player has responded
        // Wait until both players have responded
        int both_responded = 1;
        for(int i = 0; i < 2; i++) {
            if(players[i].wants_replay == -1) {
                both_responded = 0;
                break;
            }
        }
        if(!both_responded) {
            // Wait for the other player's response
            pthread_cond_wait(&replay_cond, &lock);
            continue;
        }

        // Both players have responded
        if(players[0].wants_replay && players[1].wants_replay) {
            // Reset game
            initialize_board();
            game_over = 0;
            current_player = 0;
            players[0].wants_replay = players[1].wants_replay = -1;
            // Notify players
            broadcast_message("Starting a new game!\n");
            // Send initial board
            get_board_str(board_str);
            broadcast_message(board_str);
            // Notify first player
            snprintf(turn_msg, BUFFER_SIZE, "Player %d's turn.\n", current_player + 1);
            broadcast_message(turn_msg);
        }
        else if(players[0].wants_replay && !players[1].wants_replay) {
            send_message(players[0].socket, "Your opponent declined to play again. Game over.\n");
            send_message(players[1].socket, "You declined to play again. Game over.\n");
            close(players[0].socket);
            close(players[1].socket);
            pthread_mutex_unlock(&lock);
            exit(0);
        }
        else if(!players[0].wants_replay && players[1].wants_replay) {
            send_message(players[0].socket, "You declined to play again. Game over.\n");
            send_message(players[1].socket, "Your opponent declined to play again. Game over.\n");
            close(players[0].socket);
            close(players[1].socket);
            pthread_mutex_unlock(&lock);
            exit(0);
        }
        else {
            // Both declined
            broadcast_message("Game over. Thank you for playing!\n");
            close(players[0].socket);
            close(players[1].socket);
            pthread_mutex_unlock(&lock);
            exit(0);
        }
    }
    pthread_mutex_unlock(&lock);

    // Wait for threads to finish
    for(int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    // Close server socket
    close(server_fd);
    return 0;
}

