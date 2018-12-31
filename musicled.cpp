#include "audio_data.h"
#include "color.h"
#include "fps.h"
#include "spectrum.h"
#include "video_data.h"
#include "vsync.h"

#include <alloca.h>
#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <chrono>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <iostream>
#include <list>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void* input_alsa(void* data)
{
    audio_data* audio = (audio_data*)data;
    int frames = (int)audio->frames;
    const int bpf = (audio->format / 8) * audio->channels; // bytes per frame
    const int size = frames * bpf;
    uint8_t* buffer = new uint8_t[size];

    const int stride = bpf / 2; // half-frame: bytes in a channel, or shorts in a frame
    const int loff = stride - 2; // Highest 2 bytes in the first half of a frame
    const int roff = bpf - 2; // Highest 2 bytes in the second half of a frame

    while (!audio->terminate) {
        int err = snd_pcm_readi(audio->handle, buffer, audio->frames);

        if (err == -EPIPE) {
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(audio->handle);
        } else if (err < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(err));
        } else if (err != frames) {
            fprintf(stderr, "short read, read %d of %d frames\n", err, frames);
        } else {
            double left_data[frames];
            double right_data[frames];

            // For 32 and 24 bit, we'll drop extra precision
            int16_t* left = (int16_t*)(buffer + loff);
            int16_t* right = (int16_t*)(buffer + roff);
            for (int i = 0; i < frames; i++) {
                left_data[i] = *left;
                right_data[i] = *right;
                left += stride;
                right += stride;
            }

            audio->left.write(left_data, frames);
            audio->right.write(right_data, frames);
        }
    }

    delete[] buffer;

    return NULL;
}

struct espurna {
    espurna(char* host, char* api, char* addr, audio_data* data)
        : hostname(host)
        , api_key(api)
        , audio(data)
    {
        strcpy(resolved, addr);
    }

    static espurna lookup(char* host, char* api, audio_data* data)
    {
        hostent* host_entry = gethostbyname(host);
        if (host_entry == nullptr) {
            fprintf(stderr, "Could not look up IP address for %s\n", host);
            exit(EXIT_FAILURE);
        }
        char* addr = inet_ntoa(*((in_addr*)host_entry->h_addr_list[0]));
        return espurna(host, api, addr, data);
    }

    char* hostname;
    char* api_key;
    char resolved[16];
    pthread_t p_thread;
    audio_data* audio;
};

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

void* socket_send(void* data)
{
    espurna* strip = (espurna*)data;
    audio_data* audio = strip->audio;
    std::cout << "Connecting to " << strip->hostname << " as " << strip->resolved << std::endl;

    color col;
    while (!audio->terminate) {
        VSync vsync(60); // Wait between checks

        if (col != audio->curColor) {
            col = audio->curColor;
            // std::cout << col.r << "," << col.g << "," << col.b << std::endl;

            int fd = socket_connect(strip->resolved, 80);
            if (fd != 0) {
                char buffer[1024] = { 0 };
                int len = sprintf(buffer, "GET /api/rgb?apikey=%s&value=%d,%d,%d HTTP/1.1\n\n", strip->api_key, col.r, col.g, col.b);
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

    return NULL;
}

audio_data* g_audio;

void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
        g_audio->terminate = true;
    } else {
        signal(sig_no, SIG_DFL);
        raise(sig_no);
    }
}

int main(int argc, char* argv[])
{
    // Init Audio
    audio_data audio;

    // Init FFT
    Spectrum spec(&audio);

    // Handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    g_audio = &audio;
    sigaction(SIGINT, &action, NULL);

    // Init Network
    std::list<espurna> strips;
    for (int k = 0; k + 2 < argc; k += 2)
        strips.push_back(espurna::lookup(argv[k + 1], argv[k + 2], &audio));

    // Init X11
    video_data video;

    pthread_t input_thread;
    pthread_create(&input_thread, NULL, input_alsa, (void*)&audio);
    for (espurna& strip : strips)
        pthread_create(&strip.p_thread, NULL, socket_send, (void*)&strip);

    const int framerate = 60;
    FPS fps;
    std::chrono::_V2::system_clock::time_point vstart = std::chrono::high_resolution_clock::now();
    while (!audio.terminate) {
        VSync vsync(framerate, &vstart);
        fps.tick(framerate);
        spec.process();
        video.redraw(spec);
    }

    pthread_join(input_thread, NULL);
    for (espurna& strip : strips)
        pthread_join(strip.p_thread, NULL);

    return 0;
}
