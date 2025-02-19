#!/bin/bash
gcc -Wall server.c -o server -lsqlite3
gcc -Wall client.c -o client
