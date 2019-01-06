#include "espurna.h"
#include "vsync.h"

#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

Espurna::Espurna(char* host, char* api, GlobalState* state)
    : hostname(host)
    , api_key(api)
    , global(state)
{
    hostent* host_entry = gethostbyname(host);
    if (host_entry == nullptr) {
        fprintf(stderr, "Could not look up IP address for %s\n", host);
        exit(EXIT_FAILURE);
    }
    char* addr = inet_ntoa(*((in_addr*)host_entry->h_addr_list[0]));
    strcpy(resolved, addr);
}

void Espurna::start_thread()
{
    thread = std::thread([=] { socket_send(); });
}

void Espurna::join_thread() { thread.join(); }

int socket_connect(char* host, in_port_t port)
{
    sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_aton(host, &addr.sin_addr);
    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), on;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(int));

    if (sock == -1) {
        perror("setsockopt");
        return 0;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {
        perror("connect");
        return 0;
    }
    return sock;
}

void Espurna::socket_send()
{
    std::cout << "Connecting to " << hostname << " as " << resolved << std::endl;

    Color col;
    char buffer[1024];
    while (!global->terminate) {
        VSync vsync(60); // Wait between checks

        if (col != global->cur_Color) {
            col = global->cur_Color;
            // std::cout << col.r << "," << col.g << "," << col.b << std::endl;

            int fd = socket_connect(resolved, 80);
            if (fd != 0) {
                int len = snprintf(buffer, sizeof(buffer), "GET /api/rgb?apikey=%s&value=%d,%d,%d HTTP/1.1\n\n", api_key, col.r, col.g, col.b);
                // std::cout << buffer << std::endl;

                if (write(fd, buffer, len) != -1) {
                    if (read(fd, buffer, sizeof(buffer) - 1) != 0) {
                        // std::cout << buffer << std::endl;
                    }
                }
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }
        }
    }
}
