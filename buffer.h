#ifndef BUFFER_H
#define BUFFER_H

#include <sys/socket.h>
#include <pthread.h>

typedef struct {
    int sockfd;
    struct sockaddr* address;
    int size;
    int pos;
    char* buffer;
    pthread_mutex_t lock;
} socket_buffer;

socket_buffer init_buffer(int sockfd, struct sockaddr* address, int size);

void free_buffer(socket_buffer buffer);

int send_data(socket_buffer send_buff, char* data, int len);

int recv_data(socket_buffer recv_buff, char* data, int len);

#endif