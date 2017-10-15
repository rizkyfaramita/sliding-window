#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>

typedef struct {
    char soh; // 0
    int seq; // 1,2,3,4
    char stx; // 5
    char data; // 6
    char etx; // 7
    char checksum; // 8
} segment;

int to_raw(segment seg, char* buffer) {
    buffer[0] = seg.soh;
    char* x = (char*) &seg.seq;
    buffer[1] = *x;
    buffer[2] = *(x+1);
    buffer[3] = *(x+2);
    buffer[4] = *(x+3);
    buffer[5] = seg.stx;
    buffer[6] = seg.data;
    buffer[7] = seg.etx;
    buffer[8] = seg.checksum;
    return 9;
}

void to_segment(char* raw, segment* seg) {
    seg->soh = *raw;
    seg->seq = ((int) *(raw+1))<<24 | ((int) *(raw+2))<<16 | ((int) *(raw+3))<<16 | ((int) *(raw+4));
    seg->stx = *(raw + 5);
    seg->data = *(raw + 6);
    seg->etx = *(raw + 7);
    seg->checksum = *(raw + 8);
}

void hex(char* s, int n) {
    while (n--)
        printf("%02x ", (*(s++) & 0xff));
}
 
int to_int(char* str) {
    int n = 0;
    while (*str != 0)
        n = n * 10 + ((int) (*str++) - '0');
    return n;
}

void die(char *s) {
    perror(s);
    exit(1);
}

void init_socket(int port, int *sockfd) {
    //create a UDP socket
    if ((*sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        die("Cannot creating socket instance");
    printf("Socket descriptor created\n");
    fflush(stdout);

    // zero out the structure
    struct sockaddr_in si_me;
    memset(&si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port); 
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    printf("Binding socket to port %d\n", port);
    fflush(stdout);
    if(bind(*sockfd , (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
        die("Cannot bind socket to specific port");
    printf("Socket bind on port %d\n", port);
    fflush(stdout);
}

char checksum(char x) {
    return x;
}
 
int main(int argc, char** argv) {
    if (argc < 5)
        die("Usage : ./recvfile <filename> <windowsize> <buffersize> <port>");

    int sockfd;
    int port = to_int(argv[4]);

    init_socket(port, &sockfd);
    printf("Finish initializing socket\n"); fflush(stdout);

    //keep listening for data
    while(1) {
        int len;
        char buff[256];

        len = recv(sockfd, buff, 9, 0);
        if (len < 0)
            die("Some error occured when reading buffer");
        if (*buff == '\01' && *(buff+5) == '\02' && *(buff+7) == '\03') {
            segment seg; to_segment(buff, &seg);
            printf("Segment caught\n");
            printf("  SOH      : 0x%02x\n", seg.soh);
            printf("  segnum   : 0x%02x (%d in decimal)\n", seg.seq, seg.seq);
            printf("  STX      : 0x%02x\n", seg.stx);
            printf("  data     : 0x%02x\n", seg.data & 0xff);
            printf("  ETX      : 0x%02x\n", seg.etx);
            printf("  checksum : 0x%02x\n", seg.checksum & 0xff);

            if (checksum(seg.data) != seg.checksum) {
                printf("Checksum error: calculated %02x, expected %02x\n\r", checksum(seg.data) & 0xff, seg.checksum & 0xff);
            } else {

            }

            fflush(stdout);
        }
    }

    close(sockfd);
    return 0;
}