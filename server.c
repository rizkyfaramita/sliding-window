#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "segment.h"
#include "util.h"

void init_socket(int port, int *buffer_size, int *sockfd) {
    // create a UDP socket
    if ((*sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        die("Cannot creating socket instance");
    printf("%d socket descriptor created\n", time(0));
    fflush(stdout);

    // configure buffer size
    setsockopt(*sockfd, SOL_SOCKET, SO_SNDBUF, buffer_size, sizeof(*buffer_size));
    setsockopt(*sockfd, SOL_SOCKET, SO_RCVBUF, buffer_size, sizeof(*buffer_size));

    struct sockaddr_in si_me;
    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port); 
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("%d binding socket to port %d\n", time(0), port);
    fflush(stdout);
    if(bind(*sockfd , (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
        die("Cannot bind socket to specific port");
    printf("%d socket bind success on port %d\n", time(0), port);
    fflush(stdout);
}

void send_ack_segment(int sockfd, struct sockaddr* address, char ack, int seq_num, int window_size) {
    ack_segment seg;
    char raw[16];

    seg.ack = ack ? '\06' : '\21';
    seg.next_seq = seq_num + 1;
    seg.window_size = window_size;
    ack_segment_to_raw(seg, raw);
    seg.checksum = checksum_str(raw, 6);

    printf("%d sending ack segment %d\n", time(0), seq_num);
    print_ack_segment(seg);
    fflush(stdout);

    ack_segment_to_raw(seg, raw);
    sendto(sockfd, raw, 7, 0, address, sizeof(*address));
}
 
int main(int argc, char** argv) {
    if (argc < 5)
        die("Usage : ./recvfile <filename> <windowsize> <buffersize> <port>");

    int sockfd, filed;
    char* filename = argv[1];
    int window_size = to_int(argv[2]);
    int buffer_size = to_int(argv[3]);
    int port = to_int(argv[4]);

    init_socket(port, &buffer_size, &sockfd);
    printf("%d finish initializing socket\n", time(0));
    fflush(stdout);

    if ((filed = open(filename, O_WRONLY | O_CREAT)) < 0)
        die("Cannot open file");
    
    int last_acked = -1;
    char* acked_status = (char*) malloc(window_size * sizeof(char));
    char* acked_message = (char*) malloc(window_size * sizeof(char));
    memset(acked_status, 0, window_size * sizeof(char));
    memset(acked_message, 0, window_size * sizeof(char));
    while(1) {
        int len;
        char buff[256];
        struct sockaddr_in sender_address;
        int sender_address_size = sizeof(sender_address);

        len = recvfrom(sockfd, buff, 9, 0, (struct sockaddr*) &sender_address, &sender_address_size);
        if (len < 0)
            die("Some error occured when reading buffer");

        // check, is this a segment?
        if (*buff == '\01' && *(buff+5) == '\02' && *(buff+7) == '\03') {
            segment seg;
            to_segment(buff, &seg);

            printf("%d segment %d caught ", time(0), seg.seq);
            hex(buff, 9);
            printf("\n");
            print_segment(seg);
            fflush(stdout);

            int window_index = seg.seq - last_acked - 1;
            printf("%d last_acked: %d, window_index: %d\n", time(0), last_acked, window_index);
            fflush(stdout);

            // test checksum
            if (checksum_str(buff, 8) != seg.checksum) {
                printf("%d checksum error: calculated %02x, expected %02x\n\r",
                    time(0), checksum_str(buff, 8) & 0xff, seg.checksum & 0xff);
                send_ack_segment(sockfd, (struct sockaddr*) &sender_address, 0, seg.seq + 1, window_size);
            } else if (window_index >= 0 && window_index < window_size) {
                if (seg.seq >= 0 && window_index >= 0 && window_index < window_size) {
                    acked_status[window_index] = 1;
                    acked_message[window_index] = seg.data;
                }

                int next_ack = 0;
                for (; next_ack < window_size; next_ack++)
                    if (!acked_status[next_ack])
                        break;
                
                if (next_ack > 0) {
                    write(filed, acked_message, next_ack);
                    send_ack_segment(sockfd, (struct sockaddr*) &sender_address, 1, last_acked + 1 + next_ack, window_size);

                    printf("%d writing to file and sending ack %d\n", time(0), last_acked + 1 + next_ack);
                    fflush(stdout);

                    // slide windows with next_ack
                    shl_buffer(acked_message, window_size, next_ack);
                    shl_buffer(acked_status, window_size, next_ack);
                    last_acked += next_ack;

                    printf("%d slide window to last_acked = %d\n", time(0), last_acked);
                    fflush(stdout);
                }

                if (next_ack >= window_size && seg.seq < 0) {
                    send_ack_segment(sockfd, (struct sockaddr*) &sender_address, 1, -1, window_size);
                    printf("%d send ack of EOF\n", time(0));
                    fflush(stdout);
                    break;
                }
            }
        } else {
            printf("%d not a valid segment\n", time(0));
            fflush(stdout);
        }
    }

    free(acked_status);
    free(acked_message);
    close(filed);
    close(sockfd);

    return 0;
}