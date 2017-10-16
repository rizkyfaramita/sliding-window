#!/bin/bash
gcc server.c segment.c util.c recvfile
gcc client.c segment.c util.c sendfile
