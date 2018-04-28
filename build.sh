#!/bin/bash
gcc -std=c99 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized -D_GNU_SOURCE -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L -g -O2 -MT musicled-stream.o -MD -MP -MF .deps/musicled-stream.Tpo -c -o musicled-stream.o ./stream.c
gcc -std=c99 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized -g -O2 -Wl,-rpath /usr/local/lib -o stream musicled-stream.o  -L/usr/local/lib -lpthread -lasound -lm -lfftw3 -ltinfo
