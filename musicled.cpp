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
#include <chrono>
#include <iostream>
#include <list>

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

class VSync {
private:
    std::chrono::_V2::system_clock::time_point start;
    int64_t frame_time;
    bool debug;

public:
    VSync(int frame_rate, bool dbg = false)
        : debug(dbg)
    {
        frame_time = 1e9 / frame_rate;
        start = std::chrono::high_resolution_clock::now();
    }

    ~VSync()
    {
        std::chrono::_V2::system_clock::time_point finish = std::chrono::high_resolution_clock::now();
        int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

        if (duration < frame_time) {
            timespec req = {.tv_sec = 0, .tv_nsec = 0 };
            req.tv_sec = 0;
            req.tv_nsec = frame_time - duration;
            nanosleep(&req, NULL);
        } else if (debug) {
            std::cout << "Late for " << (duration - frame_time) << " ns" << std::endl;
        }
    }
};

struct video_data {
    Display* dis;
    int screen;
    Window win;
    GC gc;
    Pixmap double_buffer = 0;
    unsigned int last_width = -1, last_height = -1;
};

void init_x(video_data* video)
{
    Display* dis = XOpenDisplay((char*)0);
    if (dis == nullptr)
        return;

    int screen = DefaultScreen(dis);
    unsigned long black = BlackPixel(dis, screen), white = WhitePixel(dis, screen);

    Window win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 648, 360, 0, black, black);
    XSetStandardProperties(dis, win, "MusicLED", "Music", None, NULL, 0, NULL);
    XSelectInput(dis, win, ExposureMask | ButtonPressMask | KeyPressMask);

    GC gc = XCreateGC(dis, win, 0, 0);

    XClearWindow(dis, win);
    XMapRaised(dis, win);
    XSetForeground(dis, gc, white);

    video->dis = dis;
    video->screen = screen;
    video->win = win;
    video->gc = gc;
};

void close_x(video_data* video)
{
    if (video->dis == nullptr)
        return;

    XFreeGC(video->dis, video->gc);
    XDestroyWindow(video->dis, video->win);
    XCloseDisplay(video->dis);
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

constexpr int M = 11;
constexpr int N = 1 << M;
constexpr int N1 = N / 2;
constexpr int HALF_N = N / 2 + 1;
constexpr int N2 = 2 * HALF_N;

struct espurna {
    espurna(char* host, char* api, char* addr, void* data)
        : hostname(host)
        , api_key(api)
        , audio(data)
    {
        strcpy(resolved, addr);
    }

    char* hostname;
    char* api_key;
    char resolved[16];
    pthread_t p_thread;
    void* audio;
};

struct color {
    int r = 0, g = 0, b = 0;
};

struct audio_data {
    int format;
    unsigned int rate;
    unsigned int channels;
    bool terminate; // shared variable used to terminate audio thread

    std::list<espurna> strip;

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };

    color curColor;

    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;

    int minK, maxK;
};

static void initialize_audio_parameters(audio_data* audio)
{
    const char* audio_source = "hw:CARD=audioinjectorpi,DEV=0";

    // alsa: open device to capture audio
    int err = snd_pcm_open(&audio->handle, audio_source, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "error opening stream: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params); // assembling params
    snd_pcm_hw_params_any(audio->handle, params); // setting defaults or something

    // interleaved mode right left right left
    snd_pcm_hw_params_set_access(audio->handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // trying to set 16bit
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_hw_params_set_format(audio->handle, params, format);

    // assuming stereo
    unsigned int channels = 2;
    snd_pcm_hw_params_set_channels(audio->handle, params, channels);

    // trying our rate
    unsigned int sample_rate = 44100;
    snd_pcm_hw_params_set_rate_near(audio->handle, params, &sample_rate, NULL);

    // number of frames per read
    snd_pcm_uframes_t frames = 256;
    snd_pcm_hw_params_set_period_size_near(audio->handle, params, &frames, NULL);

    err = snd_pcm_hw_params(audio->handle, params); // attempting to set params
    if (err < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    err = snd_pcm_prepare(audio->handle);
    if (err < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    // getting actual format
    snd_pcm_hw_params_get_format(params, &format);

    // converting result to number of bits
    switch (format) {
    case SND_PCM_FORMAT_S8:
    case SND_PCM_FORMAT_U8:
        audio->format = 8;
        break;
    case SND_PCM_FORMAT_S16_LE:
    case SND_PCM_FORMAT_S16_BE:
    case SND_PCM_FORMAT_U16_LE:
    case SND_PCM_FORMAT_U16_BE:
        audio->format = 16;
        break;
    case SND_PCM_FORMAT_S24_LE:
    case SND_PCM_FORMAT_S24_BE:
    case SND_PCM_FORMAT_U24_LE:
    case SND_PCM_FORMAT_U24_BE:
        audio->format = 24;
        break;
    case SND_PCM_FORMAT_S32_LE:
    case SND_PCM_FORMAT_S32_BE:
    case SND_PCM_FORMAT_U32_LE:
    case SND_PCM_FORMAT_U32_BE:
        audio->format = 32;
        break;
    default:
        break;
    }

    snd_pcm_hw_params_get_rate(params, &audio->rate, NULL);
    snd_pcm_hw_params_get_period_size(params, &audio->frames, NULL);
    snd_pcm_hw_params_get_channels(params, &audio->channels);
}

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

    snd_pcm_close(audio->handle);
    delete[] buffer;

    return NULL;
}

struct freq_data {
    color c;
    unsigned long ic;
    double note;
    int x;
};

// "Saw" function with specified range
inline double saw(double val, double p)
{
    double x = val / p;
    return p * fabs(x - floor(x + 0.5));
}

void precalc(freq_data* out, audio_data* audio)
{
    double maxFreq = N;
    double minFreq = audio->rate / maxFreq;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    // Notes in [36, 108] range, i.e. 6 octaves
    audio->minK = ceil(exp(35 * base) / (minFreq * fcoef));
    audio->maxK = ceil(exp(108 * base) / (minFreq * fcoef));

    for (int k = 1; k < N1; k++) {
        double frequency = k * minFreq;
        double note = log(frequency * fcoef) / base; // note = 12 * Octave + Note
        double spectre = fmod(note, 12); // spectre is within [0, 12)
        double R = saw(spectre - 6, 12);
        double G = saw(spectre - 10, 12);
        double B = saw(spectre - 2, 12);
        double mn = saw(spectre - 2, 4);

        color c;
        c.r = (int)((R - mn) * 63.75 + 0.5);
        c.g = (int)((G - mn) * 63.75 + 0.5);
        c.b = (int)((B - mn) * 63.75 + 0.5);

        freq_data f;
        f.c = c;
        f.note = note;
        f.ic = (((long)c.r) << 16) + (((long)c.g) << 8) + ((long)c.b);

        out[k] = f;
    }
}

void redraw(audio_data* audio, video_data* video, fftw_complex* out, freq_data* freq)
{
    Display* dis = video->dis;
    Window win = video->win;
    GC gc = video->gc;
    Pixmap double_buffer = video->double_buffer;
    unsigned int width, height, bw, dr;
    Window root;
    int xx, yy;
    XGetGeometry(dis, win, &root, &xx, &yy, &width, &height, &bw, &dr);

    double minNote = 34;
    double maxNote = 110;
    double kx = width / (maxNote - minNote);
    double ky = height * 0.5 / 65536.0;

    if (width != video->last_width || height != video->last_height) {
        video->last_width = width;
        video->last_height = height;
        if (double_buffer != 0) {
            XFreePixmap(dis, double_buffer);
        }
        double_buffer = XCreatePixmap(dis, win, width, height, 24);
        video->double_buffer = double_buffer;

        for (int k = 1; k < N1; k++) {
            freq[k].x = (int)((freq[k].note - minNote) * kx + 0.5);
        }
    }

    XSetForeground(dis, gc, 0);
    XFillRectangle(dis, double_buffer, gc, 0, 0, width, height);

    int minK = audio->minK, maxK = audio->maxK;
    double maxAmp = 0;
    int maxF = 0;
    int lastx = -1;
    double prevAmp = 0;

    for (int k = minK; k < maxK; k++) {
        double amp = hypot(out[k][0], out[k][1]);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }

        prevAmp = std::max(prevAmp, amp);
        int x = freq[k].x;
        if (lastx < x) {
            lastx = x + 3;
            int y = (int)(prevAmp * ky + 0.5);
            prevAmp = 0;
            XSetForeground(dis, gc, freq[k].ic);
            XDrawLine(dis, double_buffer, gc, x, height - y, x, height);
        }
    }

    audio->curColor = freq[maxF].c;

    XCopyArea(dis, double_buffer, win, gc, 0, 0, width, height, 0, 0);
    XFlush(dis);
}

void* socket_send(void* data)
{
    espurna* strip = (espurna*)data;
    audio_data* audio = (audio_data*)strip->audio;
    std::cout << "Connecting to " << strip->hostname << " as " << strip->resolved << std::endl;

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

audio_data* g_audio;

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
int main(int argc, char* argv[])
{
    // general: define variables
    pthread_t p_thread;
    int framerate = 60;
    audio_data audio;
    video_data video;

    for (int k = 0; k + 2 < argc; k += 2) {
        char* hostname = argv[k + 1];
        char* api = argv[k + 2];
        hostent* host_entry = gethostbyname(hostname);
        if (host_entry == nullptr) {
            fprintf(stderr, "Could not look up IP address for %s\n", hostname);
            exit(EXIT_FAILURE);
        }
        char* addr = inet_ntoa(*((in_addr*)host_entry->h_addr_list[0]));
        audio.strip.push_back(espurna(hostname, api, addr, &audio));
    }

    init_x(&video);
    Display* dis = video.dis;

    // general: handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    g_audio = &audio;
    sigaction(SIGINT, &action, NULL);

    // fft: planning to rock
    fftw_complex* outl = fftw_alloc_complex(HALF_N);
    fftw_complex* outr = fftw_alloc_complex(HALF_N);
    double* inl = (double*)outl;
    double* inr = (double*)outr;
    fftw_plan pl = fftw_plan_dft_r2c_1d(N, inl, outl, FFTW_MEASURE);
    fftw_plan pr = fftw_plan_dft_r2c_1d(N, inr, outr, FFTW_MEASURE);

    // input: init
    audio.format = -1;
    audio.rate = 0;
    audio.terminate = false;
    audio.channels = 2;

    initialize_audio_parameters(&audio);
    if (audio.format == -1 || audio.rate == 0) {
        fprintf(stderr, "Could not get rate and/or format, quiting...\n");
        exit(EXIT_FAILURE);
    }

    freq_data* freq = (freq_data*)malloc(N1 * sizeof(freq_data));
    precalc(freq, &audio);

    pthread_create(&p_thread, NULL, input_alsa, (void*)&audio); // starting alsamusic listener

    for (espurna& strip : audio.strip) {
        pthread_create(&strip.p_thread, NULL, socket_send, (void*)&strip);
    }

    XEvent event; /* the XEvent declaration !!! */
    KeySym key; /* a dealie-bob to handle KeyPress Events */
    char text[255]; /* a char buffer for KeyPress Events */

    int frames = 0;
    std::chrono::_V2::system_clock::time_point start = std::chrono::high_resolution_clock::now();

    while (!audio.terminate) {
        VSync vsync(framerate);

        if (frames > framerate) {
            std::chrono::_V2::system_clock::time_point finish = std::chrono::high_resolution_clock::now();
            int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
            std::cout << (framerate * 1.0e9 / duration) << " fps" << std::endl;
            frames = 0;
            start = finish;
        }

        audio.left.read(inl, N);
        if (audio.channels == 2)
            audio.right.read(inr, N);

        // process: execute FFT and sort frequency bands
        fftw_execute(pl);
        if (audio.channels == 2)
            fftw_execute(pr);

        if (dis != nullptr) {
            while (XPending(dis) > 0) {
                XNextEvent(dis, &event);

                switch (event.type) {
                case Expose:
                    // redraw(&audio, &video, outl, freq);
                    break;
                case KeyPress:
                    if (XLookupString(&event.xkey, text, 255, &key, 0) == 1 && text[0] == 'q')
                        audio.terminate = true;
                    break;
                }
            }

            redraw(&audio, &video, outl, freq);
        }

        frames++;
    }

    close_x(&video);

    pthread_join(p_thread, NULL);
    for (espurna& strip : audio.strip) {
        pthread_join(strip.p_thread, NULL);
    }
    fftw_destroy_plan(pl);
    fftw_destroy_plan(pr);
    fftw_free(outl);
    fftw_free(outr);
    free(freq);

    return 0;
}
