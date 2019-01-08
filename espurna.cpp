#include "espurna.h"
#include "vsync.h"

#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

Espurna::Espurna(std::string host, std::string api, GlobalState* state)
    : hostname(host)
    , api_key(api)
    , global(state)
{
    hostent* host_entry = gethostbyname(host.c_str());
    if (host_entry == nullptr) {
        std::cerr << "Could not look up IP address for " << host << std::endl;
        exit(EXIT_FAILURE);
    }
    resolved = inet_ntoa(*((in_addr*)host_entry->h_addr_list[0]));

    // Start the output thread immediately
    thread = std::thread([=] { socket_send(); });
}

Espurna::~Espurna() { thread.join(); }

static inline int socket_connect(std::string& host, in_port_t port)
{
    sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_aton(host.c_str(), &addr.sin_addr);
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

    Color col; // Last sent color
    char buffer[1024];
    const char* api = api_key.c_str();
    while (!global->terminate) {
        VSync vsync(60); // Wait between checks

        if (col.ic != global->cur_Color.ic) {
            col.ic = global->cur_Color.ic;

            // The color has changed, send it to the LED strip!
            int fd = socket_connect(resolved, 80);
            if (fd != 0) {
                int len = snprintf(buffer, sizeof(buffer), "GET /api/rgb?apikey=%s&value=%d,%d,%d HTTP/1.1\n\n", api, col.r, col.g, col.b);
                if (write(fd, buffer, len) != -1) {
                    if (read(fd, buffer, sizeof(buffer) - 1) != 0) {
                        // Print the response if you want
                    }
                }
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }
        }
    }
}
