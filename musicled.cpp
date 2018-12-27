#include <alloca.h>
#include <alsa/asoundlib.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <fftw3.h>
#include <getopt.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <list>
#include <chrono>
#include <iostream>

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <stdio.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/types.h>

int socket_connect(char* host, in_port_t port)
{
    struct sockaddr_in addr;
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    inet_aton(host, &addr.sin_addr);
    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP), on;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(int));

    if (sock == -1) {
        perror("setsockopt");
        return 0;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) == -1) {
        perror("connect");
        return 0;
    }
    return sock;
}

class VSync {
private:
    std::chrono::_V2::system_clock::time_point start;
    int64_t frame_time;

public:
    VSync(int frame_rate)
    {
        frame_time = 1e9 / frame_rate;
        start = std::chrono::high_resolution_clock::now();
    }

    ~VSync()
    {
        std::chrono::_V2::system_clock::time_point finish = std::chrono::high_resolution_clock::now();
        int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

        if (duration < frame_time) {
            struct timespec req = { .tv_sec = 0, .tv_nsec = 0 };
            req.tv_sec = 0;
            req.tv_nsec = frame_time - duration;
            nanosleep(&req, NULL);
        }
    }
};

Display* dis;
int screen;
Window win;
GC gc;

void init_x()
{
    dis = XOpenDisplay((char*)0);
    if (dis == nullptr)
        return;

    screen = DefaultScreen(dis);
    unsigned long black = BlackPixel(dis, screen), white = WhitePixel(dis, screen);

    win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 648, 360, 0, black, black);
    XSetStandardProperties(dis, win, "MusicLED", "Music", None, NULL, 0, NULL);
    XSelectInput(dis, win, ExposureMask | ButtonPressMask | KeyPressMask);

    gc = XCreateGC(dis, win, 0, 0);

    XClearWindow(dis, win);
    XMapRaised(dis, win);
    XSetForeground(dis, gc, white);
};

void close_x()
{
    if (dis == nullptr)
        return;

    XFreeGC(dis, gc);
    XDestroyWindow(dis, win);
    XCloseDisplay(dis);
}

template <class T> class SlidingWindow {
public:
    SlidingWindow(int n)
        : buffer(new T[n])
        , size(n)
        , pos(0)
    {
        memset(buffer, 0, n * sizeof(T));
    }

    ~SlidingWindow() { delete[] buffer; }
    /* Replace oldest N values in the circular buffer with Values */
    void write(T* values, int n)
    {
        for (int k, j = 0; j < n; j += k) {
            k = std::min(pos + (n - j), size) - pos;
            memcpy(buffer + pos, values + j, k * sizeof(T));
            pos = (pos + k) % size;
        }
    }

    /* Retrieve N latest Values */
    void read(T* values, int n)
    {
        int first = pos - n;
        while (first < 0)
            first += size;

        for (int k, j = 0; j < n; j += k) {
            k = std::min(first + (n - j), size) - first;
            memcpy(values + j, buffer + first, k * sizeof(T));
            first = (first + k) % size;
        }
    }

private:
    T* buffer;
    int size;
    int pos; // Position just after the last added value
};

// assuming stereo
#define CHANNELS_COUNT 2
#define SAMPLE_RATE 44100

const char* audio_source = "hw:CARD=audioinjectorpi,DEV=0";
constexpr int M = 11;
constexpr int N = 1 << M;
constexpr int N1 = N / 2;
constexpr int HALF_N = N / 2 + 1;
constexpr int N2 = 2 * HALF_N;

struct espurna {
    espurna(char *host, char *api) : hostname(host), api_key(api) {}

    char *hostname;
    char *api_key;
    char resolved[16];
    pthread_t p_thread;
};

struct color {
    int r, g, b;
};

struct audio_data {
    int format;
    unsigned int rate;
    int channels;
    bool terminate; // shared variable used to terminate audio thread

    std::list<espurna> strip;

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };

    color curColor;
};

struct audio_data* g_audio;

static void initialize_audio_parameters(snd_pcm_t** handle, struct audio_data* audio, snd_pcm_uframes_t* frames)
{
    // alsa: open device to capture audio
    int err = snd_pcm_open(handle, audio_source, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "error opening stream: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params); // assembling params
    snd_pcm_hw_params_any(*handle, params); // setting defaults or something
    // interleaved mode right left right left
    snd_pcm_hw_params_set_access(*handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    // trying to set 16bit
    snd_pcm_hw_params_set_format(*handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(*handle, params, CHANNELS_COUNT);
    unsigned int sample_rate = SAMPLE_RATE;
    // trying our rate
    snd_pcm_hw_params_set_rate_near(*handle, params, &sample_rate, NULL);
    // number of frames per read
    snd_pcm_hw_params_set_period_size_near(*handle, params, frames, NULL);

    err = snd_pcm_hw_params(*handle, params); // attempting to set params
    if (err < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_prepare(*handle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    // getting actual format
    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t*)&sample_rate);

    // converting result to number of bits
    if (sample_rate <= 5)
        audio->format = 16;
    else if (sample_rate <= 9)
        audio->format = 24;
    else
        audio->format = 32;

    snd_pcm_hw_params_get_rate(params, &audio->rate, NULL);
    snd_pcm_hw_params_get_period_size(params, frames, NULL);
    // snd_pcm_hw_params_get_period_time(params, &sample_rate, &dir);
}

void* input_alsa(void* data)
{
    int err;
    struct audio_data* audio = (struct audio_data*)data;
    snd_pcm_t* handle;
    snd_pcm_uframes_t frames = 256;
    initialize_audio_parameters(&handle, audio, &frames);
    const int bpf = (audio->format / 8) * CHANNELS_COUNT; // bytes per frame
    const int size = frames * bpf;
    uint8_t* buffer = new uint8_t[size];

    const int stride = bpf / 2; // half-frame: bytes in a channel, or shorts in a frame
    const int loff = stride - 2; // Highest 2 bytes in the first half of a frame
    const int roff = bpf - 2; // Highest 2 bytes in the second half of a frame

    while (!audio->terminate) {
        err = snd_pcm_readi(handle, buffer, frames);

        if (err == -EPIPE) {
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(err));
        } else if (err != (int)frames) {
            fprintf(stderr, "short read, read %d %d frames\n", err, (int)frames);
        } else {
            double left_data[frames];
            double right_data[frames];

            switch (audio->format) {
            case 33: { // One-line flag protection
                // Use the same scale, but retain higher precision
                int32_t* left = (int32_t*)buffer;
                int32_t* right = (int32_t*)(buffer + 4);
                double scale = 1 / 65536.0;
                for (unsigned int i = 0; i < frames; i++) {
                    left_data[i] = *left * scale;
                    right_data[i] = *right * scale;
                    left += 2;
                    right += 2;
                }
            } break;
            default: {
                // This is already optimal for 16 and 24 bit
                int16_t* left = (int16_t*)(buffer + loff);
                int16_t* right = (int16_t*)(buffer + roff);
                for (unsigned int i = 0; i < frames; i++) {
                    left_data[i] = (int(*left) + int(*right)) * 0.5;
                    right_data[i] = *right;
                    left += stride;
                    right += stride;
                }
            } break;
            }

            audio->left.write(left_data, frames);
            audio->right.write(right_data, frames);
        }
    }

    snd_pcm_close(handle);
    delete[] buffer;

    return NULL;
}

Pixmap double_buffer = 0;
unsigned int last_width = -1, last_height = -1;

struct freq_data {
    color c;
    unsigned long ic;
    double note;
    int x;
};

inline double clamp(double val)
{
    double x = val / 12.0;
    return 12 * fabs(x - floor(x + 0.5));
}

void precalc(freq_data *out)
{
    double maxFreq = N;
    double minFreq = SAMPLE_RATE / maxFreq;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    for (int k = 1; k < N1; k++) {
        double frequency = k * minFreq;
        double note = log(frequency * fcoef) / base; // note = 12 * Octave + Note
        double spectre = fmod(note, 12); // spectre is within [0, 12)
        double R = clamp(spectre - 6);
        double G = clamp(spectre - 10);
        double B = clamp(spectre - 2);
        double x = (spectre - 2) * 0.25;
        double mn = 4 * fabs(x - floor(x + 0.5));

        color c = {
            .r = (int)((R - mn) * 63.75 + 0.5),
            .g = (int)((G - mn) * 63.75 + 0.5),
            .b = (int)((B - mn) * 63.75 + 0.5)
        };

        freq_data f;
        f.c = c;
        f.note = note;
        f.ic = (((long)c.r) << 16) + (((long)c.g) << 8) + ((long)c.b);

        out[k] = f;
    }
}

void redraw(fftw_complex* out, freq_data* freq)
{
    unsigned int width = 648, height = 360, bw, dr;
    Window root;
    int xx, yy;
    XGetGeometry(dis, win, &root, &xx, &yy, &width, &height, &bw, &dr);

    double maxFreq = N;
    double minFreq = SAMPLE_RATE / maxFreq;
    double minNote = 34;
    double maxNote = 110;

    double kx = width / (maxNote - minNote);
    double ky = height * 0.5 / 65536.0;

    if (width != last_width || height != last_height) {
        last_width = width;
        last_height = height;
        if (double_buffer != 0) {
            XFreePixmap(dis, double_buffer);
        }
        double_buffer = XCreatePixmap(dis, win, width, height, 24);

        for (int k = 1; k < N1; k++) {
            freq[k].x = (int)((freq[k].note - minNote) * kx + 0.5);
        }
    }

    XSetForeground(dis, gc, 0);
    XFillRectangle(dis, double_buffer, gc, 0, 0, width, height);

    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = minFreq * pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9
    int minK = ceil(exp(35 * base) / fcoef);
    int maxK = ceil(exp(108 * base) / fcoef);
    double maxAmp = 0;
    int maxF;
    int lastx = -1;

    for (int k = minK; k < maxK; k++) {
        double amp = hypot(out[k][0], out[k][1]);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }

        int x = freq[k].x;
        if (lastx != x) {
            lastx = x;
            int y = (int)(amp * ky + 0.5);
            XSetForeground(dis, gc, freq[k].ic);
            XDrawLine(dis, double_buffer, gc, x, height, x, height - y);
        }
    }

    g_audio->curColor = freq[maxF].c;

    XCopyArea(dis, double_buffer, win, gc, 0, 0, width, height, 0, 0);
    XFlush(dis);
}

void* socket_send(void* data)
{
    struct audio_data* audio = g_audio;
    espurna *strip = (espurna *)data;

    struct hostent *host_entry = gethostbyname(strip->hostname);
    if (host_entry == nullptr) {
        audio->terminate = true;
    } else {
        char *addr = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 
        strcpy(strip->resolved, addr);
        std::cout << "Connecting to " << strip->hostname << " as " << strip->resolved << std::endl;
    }

    int prevR = 0, prevG = 0, prevB = 0;
    while (!audio->terminate) {
        VSync vsync(60);

        int maxR = audio->curColor.r, maxG = audio->curColor.g, maxB = audio->curColor.b;
        if (prevR != maxR || prevG != maxG || prevB != maxB) {
            int fd = socket_connect(strip->resolved, 80);
            if (fd != 0) {
                char buffer[1024] = { 0 };
                int len = sprintf(buffer, "GET /api/rgb?apikey=%s&value=%d,%d,%d HTTP/1.1\n\n", strip->api_key, maxR, maxG, maxB);
                // std::cout << buffer << std::endl;

                write(fd, buffer, len);
                if (read(fd, buffer, sizeof(buffer) - 1) != 0) {
                    // std::cout << buffer << std::endl;
                }
                shutdown(fd, SHUT_RDWR);
                close(fd);
            }

            std::cout << maxR << "," << maxG << "," << maxB << std::endl;
            prevR = maxR;
            prevG = maxG;
            prevB = maxB;
        }
    }

    return NULL;
}

// general: handle signals
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

// general: entry point
int main(int argc, char *argv[])
{
    // general: define variables
    pthread_t p_thread;
    int sleep = 0;
    int i, n;
    double inl[N2], inr[N2];
    int framerate = 60;
    bool stereo = true;

    struct timespec req = {.tv_sec = 0, .tv_nsec = 0 };
    struct audio_data audio;

//    audio.hostname = (char *)"192.168.1.222";
//    audio.api_key = (char *)"CB22BE3289153285";

    int hn = argc / 2;
    for (int i = 0; i < hn; i++) {
        int k = i * 2;
        audio.strip.push_back(espurna(argv[k + 1], argv[k + 2]));
    }

    init_x();

    if (dis == nullptr) {
        // general: handle Ctrl+C
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = &sig_handler;
        sigaction(SIGINT, &action, NULL);
    }

    // fft: planning to rock
    fftw_complex outl[HALF_N];
    fftw_plan pl = fftw_plan_dft_r2c_1d(N, inl, outl, FFTW_MEASURE);
    fftw_complex outr[HALF_N];
    fftw_plan pr = fftw_plan_dft_r2c_1d(N, inr, outr, FFTW_MEASURE);

    freq_data freq[HALF_N];
    precalc(freq);

    // input: init
    audio.format = -1;
    audio.rate = 0;
    audio.terminate = false;
    audio.channels = (stereo ? 2 : 1);

    g_audio = &audio;
    pthread_create(&p_thread, NULL, input_alsa, (void*)&audio); // starting alsamusic listener

    for (espurna& strip : audio.strip) {
        pthread_create(&strip.p_thread, NULL, socket_send, (void*)&strip);
    }

    n = 0;
    while (audio.format == -1 || audio.rate == 0) {
        req.tv_sec = 0;
        req.tv_nsec = 1000000;
        nanosleep(&req, NULL);
        n++;
        if (n > 2000) {
            fprintf(stderr, "could not get rate and/or format, "
                            "problems with audio thread? quiting...\n");
            exit(EXIT_FAILURE);
        }
    }

    XEvent event; /* the XEvent declaration !!! */
    KeySym key; /* a dealie-bob to handle KeyPress Events */
    char text[255]; /* a char buffer for KeyPress Events */

    while (!audio.terminate) {
        VSync vsync(framerate);

        // process: populate input buffer and check if input is present
        bool silence = true;

        audio.left.read(inl, N);
        if (stereo)
            audio.right.read(inr, N);

        for (i = 0; i < N; i++) {
            if (inl[i] || (stereo && inr[i]))
                silence = false;
        }

        for (i = N; i < N2; i++) {
            inl[i] = 0;
            if (stereo)
                inr[i] = 0;
        }

        if (silence)
            sleep++;
        else
            sleep = 0;

        // process: if input was present for the last 5 seconds apply FFT to it
        if (sleep < framerate * 5) {
            // process: execute FFT and sort frequency bands
            if (stereo) {
                fftw_execute(pl);
                fftw_execute(pr);
            } else {
                fftw_execute(pl);
            }
        } else { //**if in sleep mode wait and continue**//
            // wait 1 sec, then check sound again.
            req.tv_sec = 1;
            req.tv_nsec = 0;
            nanosleep(&req, NULL);
            continue;
        }

        while (dis != nullptr && XPending(dis) > 0) {
            XNextEvent(dis, &event);

            switch (event.type) {
            case Expose:
                //redraw(outl, freq);
                break;
            case KeyPress:
                if (XLookupString(&event.xkey, text, 255, &key, 0) == 1 && text[0] == 'q')
                    audio.terminate = true;
                break;
            }
        }

        if (dis != nullptr) {
            redraw(outl, freq);
        }
    }

    close_x();

    pthread_join(p_thread, NULL);
    for (espurna& strip : audio.strip) {
        pthread_join(strip.p_thread, NULL);
    }
    fftw_destroy_plan(pl);
    fftw_destroy_plan(pr);

    return 0;
}
