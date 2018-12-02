#!/bin/bash -e

time g++ -Ofast -funroll-all-loops -std=c++14 -o main main.cpp -pthread -lSDL2 -lX11 && ./main

exit 0
