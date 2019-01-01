#include "espurna.h"
#include "global.h"
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

espurna::espurna(char* host, char* api, color* col)
    : hostname(host)
    , api_key(api)
    , cur_color(col)
{
    hostent* host_entry = gethostbyname(host);
    if (host_entry == nullptr) {
        fprintf(stderr, "Could not look up IP address for %s\n", host);
        exit(EXIT_FAILURE);
    }
    char* addr = inet_ntoa(*((in_addr*)host_entry->h_addr_list[0]));
    strcpy(resolved, addr);
}

void espurna::start_thread() { pthread_create(&p_thread, NULL, run_thread, (void*)this); }

void espurna::join_thread() { pthread_join(p_thread, NULL); }

void* espurna::run_thread(void* arg)
{
    espurna* strip = (espurna*)arg;
    strip->socket_send();
    return NULL;
}

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

void espurna::socket_send()
{
    std::cout << "Connecting to " << hostname << " as " << resolved << std::endl;

    color col;
    while (!g_terminate) {
        VSync vsync(60); // Wait between checks

        if (col != *cur_color) {
            col = *cur_color;
            // std::cout << col.r << "," << col.g << "," << col.b << std::endl;

            int fd = socket_connect(resolved, 80);
            if (fd != 0) {
                char buffer[1024] = { 0 };
                int len = sprintf(buffer, "GET /api/rgb?apikey=%s&value=%d,%d,%d HTTP/1.1\n\n", api_key, col.r, col.g, col.b);
                // std::cout << buffer << std::endl;

                write(fd, buffer, len);
                if (read(fd, buffer, sizeof(buffer) - 1) != 0) {
                    // std::cout << buffer << std::endl;
                }
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }
        }
    }
}
