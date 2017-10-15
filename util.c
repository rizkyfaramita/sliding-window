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

char checksum(char x) {
    return x;
}

void die(char *s) {
    perror(s);
    exit(1);
}