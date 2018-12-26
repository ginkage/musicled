#!/bin/bash
mkdir -p .deps
g++ -std=c++11 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized -D_GNU_SOURCE -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L -g -O3 -MT musicled.o -MD -MP -MF .deps/musicled.Tpo -c -o musicled.o ./musicled.cpp
g++ -std=c++11 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized -g -O3 -Wl,-rpath /usr/local/lib -o musicled musicled.o  -L/usr/local/lib -lpthread -lasound -lm -lfftw3 -ltinfo -lX11
