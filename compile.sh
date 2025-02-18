#!/bin/bash
g++ -Wall server.cpp -o server -lsqlite3
gcc -Wall client.c -o client
