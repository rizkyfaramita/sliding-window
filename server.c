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
    printf("%d socket descriptor created\n", time());
    fflush(stdout);

    // configure buffer size
    setsockopt(*sockfd, SOL_SOCKET, SO_SNDBUF, buffer_size, sizeof(*buffer_size));
    setsockopt(*sockfd, SOL_SOCKET, SO_RCVBUF, buffer_size, sizeof(*buffer_size));

    struct sockaddr_in si_me;
    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port); 
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    printf("%d binding socket to port %d\n", time(), port);
    fflush(stdout);
    if(bind(*sockfd , (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
        die("Cannot bind socket to specific port");
    printf("%d socket bind success on port %d\n", time(), port);
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

    int sockfd, filed;
    char* filename = argv[1];
    int window_size = to_int(argv[2]);
    int buffer_size = to_int(argv[3]);
    int port = to_int(argv[4]);

    init_socket(port, &buffer_size, &sockfd);
    printf("%d finish initializing socket\n", time());
    fflush(stdout);

    if ((filed = open(filename, O_WRONLY)) < 0)
        die("Cannot open file");
    
    int last_acked = -1;
    char* acked_status = (char*) malloc(window_size * sizeof(char));
    char* acked_message = (char*) malloc(window_size * sizeof(char));
    memset(acked_status, 0, window_size * sizeof(char));
    memset(acked_message, 0, window_size * sizeof(char));
    while(1) {
        int len;
        char buff[256];

        len = recv(sockfd, buff, 9, 0);
        if (len < 0)
            die("Some error occured when reading buffer");

        // check, is this a segment?
        if (*buff == '\01' && *(buff+5) == '\02' && *(buff+7) == '\03') {
            segment seg;
            to_segment(buff, &seg);

            printf("%d segment %d caught ", time(), seg.seq);
            hex(buff, 9);
            printf("\n");
            print_segment(seg);
            fflush(stdout);

            // test checksum
            int window_index = seg.seq - last_acked + 1;
            if (checksum_str(buff, 8) != seg.checksum) {
                printf("%d checksum error: calculated %02x, expected %02x\n\r",
                    time(), checksum_str(buff, 8) & 0xff, seg.checksum & 0xff);
                send_ack_segment(sockfd, 0, seg.seq + 1, window_size);
            } else if (seg.seq || (window_index >= 0 && window_index < window_size)) {
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
                    send_ack_segment(sockfd, 1, last_acked + 1 + next_ack, window_size);

                    printf("%d writing to file and sending ack %d\n", time(), last_acked + 1 + next_ack);
                    fflush(stdout);

                    // slide windows with next_ack
                    int i;
                    for (i = 0; i < window_size; i++) {
                        acked_message[i] = i + next_ack < window_size ? acked_message[i + next_ack] : 0;
                        acked_status[i] = i + next_ack < window_size ? acked_message[i + next_ack] : 0;
                    }
                    last_acked += next_ack;

                    printf("%d slide window to last_acked = %d\n", time(), last_acked);
                    fflush(stdout);
                }

                if (next_ack >= window_size && seg.seq < 0) {
                    send_ack_segment(sockfd, 1, -1, window_size);
                    printf("%d send ack of EOF\n", time());
                    fflush(stdout);
                    break;
                }
            }
        }
    }

    free(acked_status);
    free(acked_message);
    close(filed);
    close(sockfd);
    
    return 0;
}