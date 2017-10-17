#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>

#include "buffer.h"

socket_buffer init_buffer(int sockfd, struct sockaddr* address, int size) {
    socket_buffer new_buffer;
    new_buffer.sockfd = sockfd;
    new_buffer.address = address;
    new_buffer.size = size;
    new_buffer.pos = 0;
    new_buffer.buffer = (char*) malloc(size * sizeof(char));
    pthread_mutex_init(&new_buffer.lock, NULL);
    return new_buffer;
}

void free_buffer(socket_buffer buffer) {
    free(buffer.buffer);
    pthread_mutex_destroy(&buffer.lock);
}

int send_data(socket_buffer send_buff, char* data, int len) {
    int sent = 0;
    while (len) {
        pthread_mutex_lock(&send_buff.lock);
        while (len > 0 && send_buff.pos < send_buff.size) {
            send_buff.buffer[send_buff.pos++] = *(data++);
            len--;
            sent++;
        }
        pthread_mutex_unlock(&send_buff.lock);
        if (len)
            usleep(abs(rand() % 10000));
    }
    return sent;
}

int recv_data(socket_buffer recv_buff, char* data, int len) {
    while (len) {
        pthread_mutex_lock(&recv_buff.lock);
        while (len > 0 && recv_buff.pos > 0) {
            *(data++) = *recv_buff.buffer;
            int i = 0;
            for (; i < recv_buff.pos - 1; i++)
                recv_buff.buffer[i] = recv_buff.buffer[i+1];
            len--;
            recv_buff.pos--;
        }
        pthread_mutex_unlock(&recv_buff.lock);
    }
}