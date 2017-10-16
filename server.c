#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>

#include "segment.h"
#include "util.h"

void init_socket(int port, int *buffer_size, int *sockfd) {
    // create a UDP socket
    if ((*sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        die("Cannot creating socket instance");
    printf("Socket descriptor created\n");
    fflush(stdout);

    // configure buffer size
    setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF,  &buffer_size, sizeof(buffer_size));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,  &buffer_size, sizeof(buffer_size));

    struct sockaddr_in si_me;
    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port); 
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("Binding socket to port %d\n", port);
    fflush(stdout);
    if(bind(*sockfd , (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
        die("Cannot bind socket to specific port");
    printf("Socket bind on port %d\n", port);
    fflush(stdout);
}

void send_ack_segment(int sockfd, char ack, int seq_num, int window_size) {
    ack_segment seg;
    char raw[16];

    seg.ack = ack ? '\06' : '\21';
    seg.next_seq = seq_num + 1;
    seg.window_size = window_size;
    ack_segment_to_raw(seg, raw);
    seg.checksum = checksum_str(raw, 6);

    ack_segment_to_raw(seg, raw);
    send(sockfd, raw, 7, 0);
}
 
int main(int argc, char** argv) {
    if (argc < 5)
        die("Usage : ./recvfile <filename> <windowsize> <buffersize> <port>");

    int sockfd;
    char* filename = argv[1];
    int window_size = to_int(argv[2]);
    int buffer_size = to_int(argv[3]);
    int port = to_int(argv[4]);

    init_socket(port, &buffer_size, &sockfd);
    printf("Finish initializing socket\n");
    fflush(stdout);
    
    int last_acked = -1;
    char* acked_status = (char*) malloc(window_size * sizeof(char));
    memset(acked_status, 0, window_size * sizeof(char));
    while(1) {
        int len;
        char buff[256];

        len = recv(sockfd, buff, 9, 0);
        if (len < 0)
            die("Some error occured when reading buffer");

        // check, is this a segment?
        if (*buff == '\01' && *(buff+5) == '\02' && *(buff+7) == '\03') {
            segment seg; to_segment(buff, &seg);
            printf("Segment caught\n");
            print_segment(seg);
            fflush(stdout);

            // test checksum
            int window_index = seg.next_seq - last_acked + 1;
            if (checksum_chr(seg.data) != seg.checksum) {
                printf("Checksum error: calculated %02x, expected %02x\n\r",
                    checksum_chr(seg.data) & 0xff, seg.checksum & 0xff);
                send_ack_segment(sockfd, 0, seg.seq + 1, window_size);
            } else if (window_index > 0 && window_index < window_size) {
                acked_status[window_index] = 1;

                int i = 0;
                for (; i < window_size; i++)
                    if (!acked_status[i])
                        break;
                if (i > 0) {
                    send_ack_segment(sockfd, 1, last_acked + 1 + i, window_size);
                    // slide windows
                }
            }

            fflush(stdout);
        }
    }

    free(acked_status);
    close(sockfd);
    return 0;
}