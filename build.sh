#!/bin/bash
gcc server.c segment.c util.c buffer.c -o recvfile -lpthread 
gcc client.c segment.c util.c buffer.c -o sendfile -lpthread
