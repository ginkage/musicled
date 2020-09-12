#include "flux_led.h"
#include "vsync.h"

#include <algorithm>
#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
#include <netinet/tcp.h>
#include <numeric>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

FluxLed::FluxLed(std::string host, GlobalState* state)
    : hostname(host)
    , global(state)
{
    hostent* host_entry = gethostbyname(host.c_str());
    if (host_entry == nullptr) {
        std::cerr << "Could not look up IP address for " << host << std::endl;
        exit(EXIT_FAILURE);
    }
    resolved = inet_ntoa(*((in_addr*)host_entry->h_addr_list[0]));

    // Start the output thread immediately
    thread = std::thread([this] { socket_send(); });
}

FluxLed::~FluxLed() { thread.join(); }

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

void FluxLed::close()
{
    if (_socket != 0) {
        shutdown(_socket, SHUT_RDWR);
        ::close(_socket);
        _socket = 0;
    }
}

void FluxLed::connect()
{
    close();
    _socket = socket_connect(resolved, port);
}

bool FluxLed::send_msg(const message& bytes)
{
    int len = bytes.size();
    unsigned char msg[len + 1];

    std::copy_n(bytes.begin(), len, msg);

    if (_use_csum) {
        int csum = std::accumulate(bytes.begin(), bytes.end(), 0);
        msg[len++] = csum & 0xFF;
    }

    if (send(_socket, msg, len, 0) == -1) {
        // Retry once.
        connect();
        if (send(_socket, msg, len, 0) == -1) {
            std::cerr << "Failed to send message" << std::endl;
            return false;
        }
    }

    return true;
}

bool FluxLed::read_msg(message& bytes, int expected)
{
    int remaining = expected;
    unsigned char msg[expected];

    while (remaining > 0) {
        int received = recv(_socket, msg, remaining, 0);
        if (received == 0) {
            std::cerr << "Failed to read message" << std::endl;
            return false;
        }

        remaining -= received;
    }

    int len = expected - remaining;
    bytes.resize(len);
    std::copy_n(msg, len, bytes.begin());

    return (remaining == 0);
}

void FluxLed::determine_query_len()
{
    // determine the type of protocol based on first 2 bytes.
    // if any response is recieved, use the default protocol
    message rx;
    if (send_msg({ 0x81, 0x8a, 0x8b }) && read_msg(rx, 2) && rx.size() == 2) {
        _query_len = 14;
        return;
    }

    // if no response from default received, next try the original protocol
    if (send_msg({ 0xef, 0x01, 0x77 }) && read_msg(rx, 2)) {
        if (rx[1] == 0x01) {
            protocol = PROT_LEDENET_ORIGINAL;
            _use_csum = false;
            _query_len = 11;
        } else {
            _use_csum = true;
        }
    }
}

bool FluxLed::query_state(message& state)
{
    if (_query_len == 0)
        determine_query_len();

    message msg = { 0x81, 0x8a, 0x8b };
    if (protocol == PROT_LEDENET_ORIGINAL)
        msg = { 0xef, 0x01, 0x77 };

    // connect(); // This will reopen the socket though...
    return send_msg(msg) && read_msg(state, _query_len);
}

void FluxLed::update_state()
{
    message rx;
    if (!query_state(rx) || rx.size() < _query_len) {
        _is_on = false;
        return;
    }

    // typical response:
    // pos  0  1  2  3  4  5  6  7  8  9 10
    //    66 01 24 39 21 0a ff 00 00 01 99
    //     |  |  |  |  |  |  |  |  |  |  |
    //     |  |  |  |  |  |  |  |  |  |  checksum
    //     |  |  |  |  |  |  |  |  |  warmwhite
    //     |  |  |  |  |  |  |  |  blue
    //     |  |  |  |  |  |  |  green
    //     |  |  |  |  |  |  red
    //     |  |  |  |  |  speed: 0f = highest f0 is lowest
    //     |  |  |  |  <don't know yet>
    //     |  |  |  preset pattern
    //     |  |  off(23)/on(24)
    //     |  type
    //     msg head
    //

    // response from a 5-channel LEDENET controller:
    // pos  0  1  2  3  4  5  6  7  8  9 10 11 12 13
    //    81 25 23 61 21 06 38 05 06 f9 01 00 0f 9d
    //     |  |  |  |  |  |  |  |  |  |  |  |  |  |
    //     |  |  |  |  |  |  |  |  |  |  |  |  |  checksum
    //     |  |  |  |  |  |  |  |  |  |  |  |  color mode (f0 colors were set, 0f whites, 00 all
    //     were set)
    //     |  |  |  |  |  |  |  |  |  |  |  cold-white
    //     |  |  |  |  |  |  |  |  |  |  <don't know yet>
    //     |  |  |  |  |  |  |  |  |  warmwhite
    //     |  |  |  |  |  |  |  |  blue
    //     |  |  |  |  |  |  |  green
    //     |  |  |  |  |  |  red
    //     |  |  |  |  |  speed: 0f = highest f0 is lowest
    //     |  |  |  |  <don't know yet>
    //     |  |  |  preset pattern
    //     |  |  off(23)/on(24)
    //     |  type
    //     msg head
    //

    // Devices that don't require a separate rgb/w bit
    if (rx[1] == 0x04 || rx[1] == 0x33 || rx[1] == 0x81)
        rgbwprotocol = true;

    // Devices that actually support rgbw
    if (rx[1] == 0x04 || rx[1] == 0x25 || rx[1] == 0x33 || rx[1] == 0x81)
        rgbwcapable = true;

    // Devices that use an 8-byte protocol
    if (rx[1] == 0x25 || rx[1] == 0x27 || rx[1] == 0x35)
        protocol = PROT_LEDENET;

    // Devices that use the original LEDENET protocol
    if (rx[1] == 0x01) {
        protocol = PROT_LEDENET_ORIGINAL;
        _use_csum = false;
    }

    int pattern_code = rx[3];
    int ww_level = rx[9];

    Mode mode = MODE_UNKNOWN;
    if (pattern_code == 0x61 || pattern_code == 0x62) {
        if (rgbwcapable)
            mode = MODE_COLOR;
        else if (ww_level != 0)
            mode = MODE_WW;
        else
            mode = MODE_COLOR;
    } else if (pattern_code == 0x60)
        mode = MODE_CUSTOM;
    else if (pattern_code == 0x41)
        mode = MODE_COLOR;
    // Skip preset and timer logic

    int power_state = rx[2];
    if (power_state == 0x23)
        _is_on = true;
    else if (power_state == 0x24)
        _is_on = false;

    raw_state = rx;
    _mode = mode;
}

void FluxLed::turn_on()
{
    if (protocol == PROT_LEDENET_ORIGINAL)
        send_msg({ 0xcc, 0x23, 0x33 });
    else
        send_msg({ 0x71, 0x23, 0x0f });
}

void FluxLed::turn_off()
{
    if (protocol == PROT_LEDENET_ORIGINAL)
        send_msg({ 0xcc, 0x24, 0x33 });
    else
        send_msg({ 0x71, 0x24, 0x0f });
}

void FluxLed::set_rgb(Color c)
{
    bool persist = true;
    // sample message for original LEDENET protocol (w/o checksum at end)
    //  0  1  2  3  4
    // 56 90 fa 77 aa
    //  |  |  |  |  |
    //  |  |  |  |  terminator
    //  |  |  |  blue
    //  |  |  green
    //  |  red
    //  head

    // sample message for 8-byte protocols (w/ checksum at end)
    //  0  1  2  3  4  5  6
    // 31 90 fa 77 00 00 0f
    //  |  |  |  |  |  |  |
    //  |  |  |  |  |  |  terminator
    //  |  |  |  |  |  write mask / white2 (see below)
    //  |  |  |  |  white
    //  |  |  |  blue
    //  |  |  green
    //  |  red
    //  persistence (31 for true / 41 for false)
    //
    // byte 5 can have different values depending on the type
    // of device:
    // For devices that support 2 types of white value (warm and cold
    // white) this value is the cold white value. These use the LEDENET
    // protocol. If a second value is not given, reuse the first white value.
    //
    // For devices that cannot set both rbg and white values at the same time
    // (including devices that only support white) this value
    // specifies if this command is to set white value (0f) or the rgb
    // value (f0).
    //
    // For all other rgb and rgbw devices, the value is 00

    // sample message for 9-byte LEDENET protocol (w/ checksum at end)
    //  0  1  2  3  4  5  6  7
    // 31 bc c1 ff 00 00 f0 0f
    //  |  |  |  |  |  |  |  |
    //  |  |  |  |  |  |  |  terminator
    //  |  |  |  |  |  |  write mode (f0 colors, 0f whites, 00 colors & whites)
    //  |  |  |  |  |  cold white
    //  |  |  |  |  warm white
    //  |  |  |  blue
    //  |  |  green
    //  |  red
    //  persistence (31 for true / 41 for false)
    //

    message msg;

    // The original LEDENET protocol
    if (protocol == PROT_LEDENET_ORIGINAL)
        msg = { 0x56, c.r, c.g, c.b, 0xaa };
    else {
        // all other devices

        // assemble the message
        if (persist)
            msg.push_back(0x31);
        else
            msg.push_back(0x41);

        msg.push_back(c.r);
        msg.push_back(c.g);
        msg.push_back(c.b);
        msg.push_back(0); // w

        if (protocol == PROT_LEDENET) {
            // LEDENET devices support two white outputs for cold and warm. We set
            // the second one here - if we're only setting a single white value,
            // we set the second output to be the same as the first
            msg.push_back(0); // w2
        }

        // write mask, default to writing color and whites simultaneously
        int write_mask = 0x00;
        // rgbwprotocol devices always overwrite both color & whites
        if (!rgbwprotocol) {
            // Mask out whites
            write_mask |= 0xf0;
        }

        msg.push_back(write_mask);

        // Message terminator
        msg.push_back(0x0f);
    }

    // send the message
    send_msg(msg);
}

void FluxLed::socket_send()
{
    std::cout << "Connecting to " << hostname << " as " << resolved << std::endl;

    connect();
    update_state();
    turn_on();

    Color col; // Last sent color
    hires_clock::time_point vcheck = hires_clock::now(), vsend = vcheck;
    while (!global->terminate) {
        VSync vsync(60, &vcheck); // Wait between checks
        if (col.ic != global->cur_color.ic) {
            col.ic = global->cur_color.ic;
            VSync vsync(2 * global->bpm / 60.0, &vsend); // Wait between sending
            // The color has changed, send it to the LED strip!
            set_rgb(col);
        }
    }

    turn_off();
    close();
}
