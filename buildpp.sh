#!/bin/bash
g++ -std=c++11 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized -D_GNU_SOURCE -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L -g -O2 -MT musicled-streampp.o -MD -MP -MF .deps/musicled-streampp.Tpo -c -o musicled-streampp.o ./stream.cpp
g++ -std=c++11 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized -g -O2 -Wl,-rpath /usr/local/lib -o stream musicled-streampp.o  -L/usr/local/lib -lpthread -lasound -lm -lfftw3 -ltinfo
