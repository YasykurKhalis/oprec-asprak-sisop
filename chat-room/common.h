// common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PORT 8888
#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define NAME_LEN 32

// Struktur untuk menampung data client
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LEN];
    char room[NAME_LEN]; // Menyimpan nama room
} client_t;

// mutex mencegah race condition
extern pthread_mutex_t clients_mutex;

#endif
