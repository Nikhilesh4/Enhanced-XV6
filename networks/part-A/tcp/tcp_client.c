// tcp_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 12345
#define BUFFER_SIZE 1024
int my_turn = 0; 
int sock = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Function to receive messages from server
void *receive_handler(void *arg)
{
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0)
        {
            printf("Disconnected from server.\n");
            exit(1);
        }

        buffer[bytes] = '\0';
           if (strcmp(buffer, "YOUR_TURN") == 0) {
            my_turn = 1; // It's this client's turn
            printf("It's your turn!\n");
        } else if (strcmp(buffer, "It's not your turn") == 0) {
            my_turn = 0; // Not this client's turn
            printf("It's not your turn. Wait for your turn.\n");
        } else {
            // Display any other message from the server
            printf("%s", buffer);
        }
    }
    return NULL;
}

int main()
{
    struct sockaddr_in serv_addr;
    pthread_t recv_thread;
    char input[BUFFER_SIZE];
    char ip_address[INET_ADDRSTRLEN]; // Buffer to hold the IP address

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("Socket creation error\n");
        return -1;
    }

    // Prompt for server IP address
    printf("Enter the server IP address: ");
    fgets(ip_address, sizeof(ip_address), stdin);
    ip_address[strcspn(ip_address, "\n")] = 0; // Remove newline character

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 addresses from text to binary form
    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0)
    {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("Connection Failed\n");
        return -1;
    }

    // Create thread to receive messages
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0)
    {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Main loop to send user input
    while (1)
    {
        fgets(input, BUFFER_SIZE, stdin);
        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        if (send(sock, input, strlen(input), 0) < 0)
        {
            printf("Send failed\n");
            break;
        }
    }

    // Cleanup
    pthread_join(recv_thread, NULL);
    close(sock);
    return 0;
}
