// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdint.h>
#include <errno.h>

#define CHUNK_SIZE 10
#define MAX_CHUNKS 100
#define PORT 12343
#define TIMEOUT 100000  // Timeout duration in microseconds (0.1 seconds)
#define WINDOW_SIZE 5    // Number of chunks that can be sent without waiting for ACKs

// Define the packet structure
typedef struct {
    uint32_t seq_num;       // Sequence number
    uint32_t total_chunks;  // Total number of chunks
    char data[CHUNK_SIZE];  // Chunk data
} Packet;

// Define the acknowledgment structure
typedef struct {
    uint32_t seq_num;  // Sequence number of the received chunk
} AckPacket;

// Structure to track the status of each chunk
typedef struct {
    int acknowledged;          // 1 if ACK received, 0 otherwise
    struct timeval sent_time;  // Time when the chunk was sent
} ChunkStatus;

// Global variables
int sockfd;
struct sockaddr_in client_addr;
socklen_t addr_len = sizeof(client_addr);
ChunkStatus chunk_status[MAX_CHUNKS];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Function to get current time
void get_current_time(struct timeval *tv) {
    gettimeofday(tv, NULL);
}

// Function to calculate time difference in microseconds
long time_diff_microseconds(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000000L + (end->tv_usec - start->tv_usec);
}

// Function to send a message using sliding window
void *send_message(void *arg) {
    char *message = (char *)arg;
    size_t message_len = strlen(message);
    uint32_t total_chunks = (message_len / CHUNK_SIZE) + (message_len % CHUNK_SIZE ? 1 : 0);
    Packet packets[MAX_CHUNKS];

    // Initialize packets
    for (uint32_t i = 0; i < total_chunks; i++) {
        packets[i].seq_num = i;
        packets[i].total_chunks = total_chunks;

        size_t offset = i * CHUNK_SIZE;
        size_t chunk_size = (i < total_chunks - 1) ? CHUNK_SIZE : (message_len - offset);
        strncpy(packets[i].data, message + offset, chunk_size);
        packets[i].data[chunk_size] = '\0'; // Null-terminate
    }

    uint32_t base = 0;  // Base of the window
    uint32_t next_seq = 0;  // Next sequence number to send

    while (base < total_chunks) {
        pthread_mutex_lock(&mutex);
        // Send packets within the window
        while (next_seq < base + WINDOW_SIZE && next_seq < total_chunks) {
            // Check if the packet has not been sent or needs retransmission
            if (chunk_status[next_seq].acknowledged == 0 && 
                (time_diff_microseconds(&chunk_status[next_seq].sent_time, &(struct timeval){0}) == 0)) {
                // Send the packet
                ssize_t sent_bytes = sendto(sockfd, &packets[next_seq], sizeof(Packet), 0,
                                            (struct sockaddr *)&client_addr, addr_len);
                if (sent_bytes < 0) {
                    perror("Server: sendto failed");
                } else {
                    get_current_time(&chunk_status[next_seq].sent_time);
                    printf("Server: Sent chunk %d/%d: %s\n", next_seq + 1, total_chunks, packets[next_seq].data);
                }
            }
            next_seq++;
        }
        pthread_mutex_unlock(&mutex);

        // Set up select for ACK reception with timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT;

        int select_result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (select_result < 0) {
            perror("Server: select failed");
            break;
        } else if (select_result == 0) {
            // Timeout: Check for any packets that need retransmission
            pthread_mutex_lock(&mutex);
            struct timeval current_time;
            get_current_time(&current_time);
            for (uint32_t i = base; i < next_seq && i < total_chunks; i++) {
                if (chunk_status[i].acknowledged == 0) {
                    long elapsed = time_diff_microseconds(&chunk_status[i].sent_time, &current_time);
                    if (elapsed >= TIMEOUT) {
                        // Resend the packet
                        ssize_t sent_bytes = sendto(sockfd, &packets[i], sizeof(Packet), 0,
                                                    (struct sockaddr *)&client_addr, addr_len);
                        if (sent_bytes < 0) {
                            perror("Server: sendto failed (retransmission)");
                        } else {
                            get_current_time(&chunk_status[i].sent_time);
                            printf("Server: Resent chunk %d/%d: %s\n", i + 1, total_chunks, packets[i].data);
                        }
                    }
                }
            }
            pthread_mutex_unlock(&mutex);
        } else if (FD_ISSET(sockfd, &read_fds)) {
            // Receive ACK
            AckPacket ack;
            ssize_t ack_bytes = recvfrom(sockfd, &ack, sizeof(AckPacket), 0, NULL, NULL);
            if (ack_bytes < 0) {
                perror("Server: recvfrom failed (ACK)");
                continue;
            }

            pthread_mutex_lock(&mutex);
            if (ack.seq_num < total_chunks) {
                if (chunk_status[ack.seq_num].acknowledged == 0) {
                    chunk_status[ack.seq_num].acknowledged = 1;
                    printf("Server: Received ACK for chunk %d/%d\n", ack.seq_num + 1, total_chunks);
                    if (ack.seq_num == base) {
                        // Slide the window forward
                        while (base < total_chunks && chunk_status[base].acknowledged) {
                            base++;
                        }
                    }
                }
            } else {
                printf("Server: Received ACK for invalid chunk %d\n", ack.seq_num + 1);
            }
            pthread_mutex_unlock(&mutex);
        }
    }

    printf("Server: All chunks acknowledged. Send thread exiting.\n");
    return NULL;
}

// Function to receive a message in chunks
int receive_message(int sockfd, struct sockaddr_in *client_addr, socklen_t addr_len, char *complete_message) {
    Packet packet;
    AckPacket ack;
    char *received_chunks[MAX_CHUNKS] = {NULL};
    uint32_t total_chunks = 0;
    int message_complete = 0;

    while (!message_complete) {
        ssize_t received_bytes = recvfrom(sockfd, &packet, sizeof(packet), 0, (struct sockaddr *)client_addr, &addr_len);
        if (received_bytes < 0) {
            perror("Server: recvfrom failed");
            return -1;
        }

        // Check for termination signal
        if (packet.total_chunks == 0 && packet.seq_num == 0) {
            printf("Server: Received termination signal from client.\n");
            return 1;  // Indicate termination
        }

        // Update total_chunks
        if (packet.total_chunks > total_chunks) {
            total_chunks = packet.total_chunks;
            printf("Server: Expecting %d chunks.\n", total_chunks);
        }

        // Store the received chunk
        if (packet.seq_num < MAX_CHUNKS && received_chunks[packet.seq_num] == NULL) {
            received_chunks[packet.seq_num] = strdup(packet.data);
            printf("Server: Received chunk %d/%d: %s\n", packet.seq_num + 1, total_chunks, received_chunks[packet.seq_num]);
        } else {
            printf("Server: Duplicate or out-of-range chunk %d received. Ignoring.\n", packet.seq_num + 1);
        }

        // Send ACK
        ack.seq_num = packet.seq_num;
        ssize_t sent_ack = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *)client_addr, addr_len);
        if (sent_ack < 0) {
            perror("Server: sendto failed (ACK)");
            return -1;
        }
        printf("Server: Sent ACK for chunk %d/%d\n", ack.seq_num + 1, total_chunks);

        // Check if all chunks have been received
        message_complete = 1;
        for (uint32_t i = 0; i < total_chunks; i++) {
            if (received_chunks[i] == NULL) {
                message_complete = 0;
                break;
            }
        }
    }

    // Reconstruct the complete message
    complete_message[0] = '\0';
    for (uint32_t i = 0; i < total_chunks; i++) {
        strcat(complete_message, received_chunks[i]);
        free(received_chunks[i]);  // Free memory
    }
    printf("Server: Final Message from Client: %s\n", complete_message);
    return 0;
}

int main() {
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Server: Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Server address configuration
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Server: Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server: Running and waiting for client on port %d...\n", PORT);

    while (1) {
        // Receive message from client
        printf("\nServer: Waiting to receive message from client...\n");
        char complete_message[CHUNK_SIZE * MAX_CHUNKS] = {0};
        int recv_status = receive_message(sockfd, &client_addr, addr_len, complete_message);
        if (recv_status == 1) {
            printf("Server: Client has terminated the connection.\n");
            break;
        } else if (recv_status == -1) {
            printf("Server: Error while receiving message. Continuing...\n");
            continue;
        }

        // Prompt server operator to send a message
        printf("Server: Enter message to send to client (or 'exit' to terminate): ");
        if (fgets(complete_message, sizeof(complete_message), stdin) == NULL) {
            perror("Server: fgets failed");
            continue;
        }

        // Remove newline character
        size_t len = strlen(complete_message);
        if (len > 0 && complete_message[len - 1] == '\n') {
            complete_message[len - 1] = '\0';
            len--;
        }

        // Check for termination
        if (strncmp(complete_message, "exit", 4) == 0) {
            // Send termination signal to client
            Packet term_packet;
            term_packet.seq_num = 0;
            term_packet.total_chunks = 0;
            memset(term_packet.data, 0, CHUNK_SIZE);
            if (sendto(sockfd, &term_packet, sizeof(term_packet), 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
                perror("Server: sendto failed (termination)");
            }
            printf("Server: Termination signal sent to client. Exiting.\n");
            break;
        }

        // Reset chunk status
        memset(chunk_status, 0, sizeof(chunk_status));

        // Create a separate thread to send the message
        pthread_t send_thread;
        char *message_to_send = strdup(complete_message); // Allocate memory for the message
        if (pthread_create(&send_thread, NULL, send_message, (void *)message_to_send) != 0) {
            perror("Server: Failed to create send thread");
            free(message_to_send);
            continue;
        }

        // Wait for the send thread to finish
        pthread_join(send_thread, NULL);
        free(message_to_send);
    }

    close(sockfd);
    printf("Server: Socket closed. Exiting.\n");
    return 0;
}
