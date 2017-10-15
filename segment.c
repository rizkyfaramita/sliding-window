#include <stdio.h>

#include "segment.h"

int segment_to_raw(segment seg, char* buffer) {
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

int ack_segment_to_raw(ack_segment seg, char* buffer) {
    buffer[0] = seg.ack;
    char* x = (char*) &seg.next_seq;
    buffer[1] = *x;
    buffer[2] = *(x+1);
    buffer[3] = *(x+2);
    buffer[4] = *(x+3);
    buffer[5] = seg.window_size;
    buffer[6] = seg.checksum;
    return 7;
}

void to_segment(char* raw, segment* seg) {
    seg->soh = *raw;
    seg->seq = ((int) *(raw+1))<<24 | ((int) *(raw+2))<<16 | ((int) *(raw+3))<<16 | ((int) *(raw+4));
    seg->stx = *(raw + 5);
    seg->data = *(raw + 6);
    seg->etx = *(raw + 7);
    seg->checksum = *(raw + 8);
}

void print_segment(segment seg) {
    printf("  SOH      : 0x%02x\n", seg.soh);
    printf("  segnum   : 0x%02x (%d in decimal)\n", seg.seq, seg.seq);
    printf("  STX      : 0x%02x\n", seg.stx);
    printf("  data     : 0x%02x\n", seg.data & 0xff);
    printf("  ETX      : 0x%02x\n", seg.etx);
    printf("  checksum : 0x%02x\n", seg.checksum & 0xff);
}