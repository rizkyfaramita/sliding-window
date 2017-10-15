#ifndef UTIL_H
#define UTIL_H

void hex(char* s, int n);
 
int to_int(char* str);

void die(char *s);

char checksum_chr(char x);

char checksum_str(char* x, int len);

#endif