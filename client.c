#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "segment.h"
#include "util.h"

const int TIMEOUT = 1;

void init_socket(int *buffer_size, int *sockfd) {
    // create a UDP socket
    if ((*sockfd=socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP)) == -1)
        die("Cannot creating socket instance");
    printf("%d socket descriptor created\n", time(0));
    fflush(stdout);

    // configure buffer size
    setsockopt(*sockfd, SOL_SOCKET, SO_SNDBUF, buffer_size, sizeof(*buffer_size));
    setsockopt(*sockfd, SOL_SOCKET, SO_RCVBUF, buffer_size, sizeof(*buffer_size));
}

void init_address(int port, char* ip_address, struct sockaddr_in *address) {
    memset(address, 0, sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(port); 
    inet_aton(ip_address , &address->sin_addr);
}

char send_segment(int *sockfd, struct sockaddr_in *address, int seqnum, char data) {
    segment seg;
    seg.soh = '\01';
    seg.seq = seqnum;
    seg.stx = '\02';
    seg.data = data;
    seg.etx = '\03';
    
    char buffer[10];
    segment_to_raw(seg, buffer);
    seg.checksum = checksum_str(buffer, 8);
    segment_to_raw(seg, buffer);

    print_segment(seg);
    return sendto(*sockfd, buffer, 9, 0, (struct sockaddr*) address, sizeof(*address)) >= 9;
}

int readfd(int fd) {
    char buff[2];
    int ret = read(fd, buff, 1);
    if (ret == 0)
        return EOF;
    else
        return (int) buff[0];
}
 
int main(int argc, char** argv) {
    if (argc < 6)
        die("Usage : ./sendfile <filename> <windowsize> <buffersize> <destination_ip> <destination_port>");

    int sockfd, filed;
    struct sockaddr_in sock_address;
    char* filename = argv[1];
    int window_size = to_int(argv[2]);
    int buffer_size = to_int(argv[3]);
    char* recv_addr = argv[4];
    int port = to_int(argv[5]);

    init_socket(&buffer_size, &sockfd);
    printf("%d finish initializing socket\n", time(0));
    fflush(stdout);

    init_address(port, recv_addr, &sock_address);
    printf("%d finish initializing socket address\n", time(0));
    fflush(stdout);

    if ((filed = open(filename, O_RDONLY)) < 0)
        die("Cannot open file");

    char *acked_sign = (char*) malloc(window_size * sizeof(char));
    char *message_buff = (char*) malloc(window_size * sizeof(char));
    int *message_time = (int*) malloc(window_size * sizeof(int));
    memset(acked_sign, 0, window_size * sizeof(char));
    memset(message_buff, 0, window_size * sizeof(char));
    memset(message_time, 0, window_size * sizeof(int));

    int last_acked = -1;
    int last_not_acked = 0;
    int last_char_read = 0;
    for (; last_not_acked < window_size && (last_char_read = readfd(filed)) != EOF; last_not_acked++) {
        message_buff[last_not_acked] = last_char_read;
        message_time[last_not_acked] = 0;
    }
    last_not_acked--;
    
    while(1) {
        int len;
        char buff[256];

        len = recv(sockfd, buff, 7, 0);
        if (len > 0)
            printf("%d %d\n", time(0), len), hex(buff, len);
        if (len == 7) {
            ack_segment seg;
            to_ack_segment(buff, &seg);
            
            printf("%d ack %d caught ", time(0), seg.next_seq);
            hex(buff, 7);
            printf("\n");
            print_ack_segment(seg);
            fflush(stdout);
            
            char checks = checksum_str(buff, 6);
            if (checks != seg.checksum) {
                printf("%d checksum error: calculated %02x, expected %02x\n\r",
                    time(0), checks & 0xff, seg.checksum & 0xff);
            } else if (seg.ack == '\06') {
                if (seg.next_seq > last_acked && seg.next_seq <= last_not_acked) {
                    int i = 0;
                    for (; i < seg.next_seq - last_acked; i++)
                        acked_sign[i] = 1;
                    while (last_acked < seg.next_seq - 1) {
                        last_acked++;
                        
                        if (last_char_read != EOF) {
                            last_char_read = readfd(filed);
                            last_not_acked++;
                        }

                        shl_buffer(acked_sign, window_size, 1);
                        shl_buffer(message_buff, window_size, 1);
                        shl_bufferl(message_time, window_size, 1);

                        message_buff[last_not_acked - last_acked - 1] = last_char_read;
                    }
                } else {
                    printf("%d segment is discarded because not in window range\n", time(0));
                    fflush(stdout);
                }
            } else if (seg.ack == '\21') {
                message_time[seg.next_seq - last_acked - 1] = -1;
                printf("%d segment %d is nacked\n", time(0), seg.next_seq);
                fflush(stdout);
            } else {
                printf("%d not a valid ack segment\n", time(0));
                fflush(stdout);
            }
        }

        // sending all segment that need to be sent.
        int now = time(0);
        int i;
        for (i = 0; i < last_not_acked - last_acked; i++)
            if (now - message_time[i] >= TIMEOUT && !acked_sign[i]) {
                int seqnum = i + last_acked + 1;

                if (message_time[i] == -1) {
                    printf("%d segment %d is nacked\n", time(0), seqnum);
                    fflush(stdout);
                } else if (message_time[i] > 0) {
                    printf("%d segment %d is timed out\n", time(0), seqnum);
                    fflush(stdout);
                }
                printf("%d sending segment %d\n", time(0), seqnum);
                fflush(stdout);

                int send_seq_num = seqnum;
                if (last_char_read == EOF && i >= last_not_acked - last_acked - 1)
                    send_seq_num = -1;
                if (!send_segment(&sockfd, &sock_address, send_seq_num, message_buff[i])) {
                    printf("%d failed sending segment %d because the buffer is full\n", time(0), seqnum);
                    fflush(stdout);
                } else {
                    message_time[i] = time(0);
                    printf("%d segment %d sent\n", time(0), seqnum);
                    fflush(stdout);
                }
            }

        char all_acked = 1;
        for (i = 0; i < last_not_acked - last_acked && all_acked; i++)
            if (!acked_sign[i])
                all_acked = 0;
        if (all_acked && last_char_read == EOF)
            break;
    }
    
    free(acked_sign);
    free(message_buff);
    close(filed);
    close(sockfd);

    return 0;
}