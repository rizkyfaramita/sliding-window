#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <arpa/inet.h>
#include <sys/socket.h>
 
#define BUFLEN 512

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
    char* x = &seg.seq;
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
 
int main(int argc, char** argv) {
    if (argc < 5)
        die("Usage : ./recvfile <filename> <windowsize> <buffersize> <port>");
    printf("a\n"); fflush(stdout);

    int sockfd, port = to_int(argv[4]), recv_len;
    struct sockaddr_in si_me, si_other;
    char buf[BUFLEN];
    int slen = sizeof(si_other);

    //create a UDP socket
    if ((sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        die("Cannot creating socket instance");
    printf("Socket descriptor created\n"); fflush(stdout);

    // zero out the structure
    memset(&si_me, 0, sizeof(si_me));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port); 
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind socket to port
    if(bind(sockfd , (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
        die("Cannot bind socket to specific port");
    printf("Socket bind\n"); fflush(stdout);

    //keep listening for data
    while(1) {
        printf("Waiting for data...");
        fflush(stdout);

        //try to receive some data, this is a blocking call
        if ((recv_len = recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*) &si_other, &slen)) == -1)
            die("Some error occured when recvfrom() is called");

        //print details of the client/peer and the data received
        printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
        printf("Data: %s\n" , buf);

        //now reply the client with the same data
        if (sendto(sockfd, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
            die("Some error occured when sendto() is called");
    }

    close(sockfd);
    return 0;
}