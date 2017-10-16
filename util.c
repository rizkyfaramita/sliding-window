#include <stdio.h>
#include <stdlib.h>

#include "util.h"

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

char checksum_chr(char x) {
    return x;
}

char checksum_str(char* x, int len) {
    int n = 0;
    while (len--)
        n += checksum_chr(*(x++));
    return (char) n;
}

void shl_buffer(char* buffer, int len, int num) {
    int i;
    for (i = 0; i < len; i++)
        buffer[i] = i + num < len ? buffer[i+num] : 0;
}

void shl_bufferl(int* buffer, int len, int num) {
    int i;
    for (i = 0; i < len; i++)
        buffer[i] = i + num < len ? buffer[i+num] : 0;
}

void die(char *s) {
    perror(s);
    exit(1);
}