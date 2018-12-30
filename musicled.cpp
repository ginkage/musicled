#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <alloca.h>
#include <alsa/asoundlib.h>
#include <arpa/inet.h>
#include <chrono>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <fftw3.h>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

class VSync {
private:
    std::chrono::_V2::system_clock::time_point start;
    std::chrono::_V2::system_clock::time_point* pstart;
    int64_t frame_time;

public:
    VSync(int frame_rate, std::chrono::_V2::system_clock::time_point* prev = nullptr)
        : pstart(prev)
        , frame_time(1e9 / frame_rate)
    {
        start = (pstart != nullptr) ? (*pstart) : std::chrono::high_resolution_clock::now();
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

            if (pstart != nullptr)
                *pstart = start + std::chrono::nanoseconds(frame_time);
        } else if (pstart != nullptr)
            *pstart = finish;
    }
};

class FPS {
private:
    int frames;
    std::chrono::_V2::system_clock::time_point start;

public:
    FPS()
        : frames(-1)
        , start(std::chrono::high_resolution_clock::now())
    {
    }

    void tick(int framerate)
    {
        if (++frames >= framerate) {
            std::chrono::_V2::system_clock::time_point finish = std::chrono::high_resolution_clock::now();
            int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
            std::cout << (frames * 1.0e9 / duration) << " fps" << std::endl;
            frames = 0;
            start = finish;
        }
    }
};

struct video_data {
    Display* dis;
    int screen;
    Window win;
    GC gc;
    Atom close;
    Pixmap double_buffer = 0;
    unsigned int last_width = -1, last_height = -1;
};

void init_x(video_data& video)
{
    Display* dis = video.dis = XOpenDisplay((char*)0);
    if (dis == nullptr)
        return;

    int screen = video.screen = DefaultScreen(dis);
    unsigned long black = BlackPixel(dis, screen), white = WhitePixel(dis, screen);

    Window win = video.win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 648, 360, 0, black, black);
    XSetStandardProperties(dis, win, "MusicLED", "Music", None, NULL, 0, NULL);
    XSelectInput(dis, win, ExposureMask | ButtonPressMask | KeyPressMask);

    GC gc = video.gc = XCreateGC(dis, win, 0, 0);

    XClearWindow(dis, win);
    XMapRaised(dis, win);
    XSetForeground(dis, gc, white);

    Atom close = video.close = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dis, win, &close, 1);
};

void close_x(video_data& video)
{
    if (video.dis == nullptr)
        return;

    XFreeGC(video.dis, video.gc);
    XDestroyWindow(video.dis, video.win);
    XCloseDisplay(video.dis);
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

struct color {
    int r{ 0 };
    int g{ 0 };
    int b{ 0 };

    bool operator!=(const color& that) const { return r != that.r || g != that.g || b != that.b; }
};

struct audio_data {
public:
    audio_data() { init(); }

    ~audio_data() { shutdown(); }

    int format{ -1 };
    unsigned int rate{ 0 };
    unsigned int channels{ 2 };

    snd_pcm_t* handle;
    snd_pcm_uframes_t frames;

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };

    bool terminate{ false }; // shared variable used to terminate audio thread
    color curColor;

private:
    void init();
    void shutdown();
};

void audio_data::init()
{
    const char* audio_source = "hw:CARD=audioinjectorpi,DEV=0";

    // alsa: open device to capture audio
    int err = snd_pcm_open(&handle, audio_source, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "error opening stream: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params); // assembling params
    snd_pcm_hw_params_any(handle, params); // setting defaults or something

    // interleaved mode right left right left
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // trying to set 16bit
    snd_pcm_format_t pcm_format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_hw_params_set_format(handle, params, pcm_format);

    // assuming stereo
    unsigned int pcm_channels = 2;
    snd_pcm_hw_params_set_channels(handle, params, pcm_channels);

    // trying our rate
    unsigned int pcm_rate = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &pcm_rate, NULL);

    // number of frames per read
    snd_pcm_uframes_t pcm_frames = 256;
    snd_pcm_hw_params_set_period_size_near(handle, params, &pcm_frames, NULL);

    err = snd_pcm_hw_params(handle, params); // attempting to set params
    if (err < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    err = snd_pcm_prepare(handle);
    if (err < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    // getting actual format
    snd_pcm_hw_params_get_format(params, &pcm_format);

    // converting result to number of bits
    switch (pcm_format) {
    case SND_PCM_FORMAT_S8:
    case SND_PCM_FORMAT_U8:
        format = 8;
        break;
    case SND_PCM_FORMAT_S16_LE:
    case SND_PCM_FORMAT_S16_BE:
    case SND_PCM_FORMAT_U16_LE:
    case SND_PCM_FORMAT_U16_BE:
        format = 16;
        break;
    case SND_PCM_FORMAT_S24_LE:
    case SND_PCM_FORMAT_S24_BE:
    case SND_PCM_FORMAT_U24_LE:
    case SND_PCM_FORMAT_U24_BE:
        format = 24;
        break;
    case SND_PCM_FORMAT_S32_LE:
    case SND_PCM_FORMAT_S32_BE:
    case SND_PCM_FORMAT_U32_LE:
    case SND_PCM_FORMAT_U32_BE:
        format = 32;
        break;
    default:
        break;
    }

    snd_pcm_hw_params_get_rate(params, &rate, NULL);
    snd_pcm_hw_params_get_period_size(params, &frames, NULL);
    snd_pcm_hw_params_get_channels(params, &channels);

    if (format == -1 || rate == 0) {
        fprintf(stderr, "Could not get rate and/or format, quiting...\n");
        exit(EXIT_FAILURE);
    }
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

    delete[] buffer;

    return NULL;
}

void audio_data::shutdown() { snd_pcm_close(handle); }

struct freq_data {
    color c;
    unsigned long ic;
    double note;
    int x;
};

class FFTData {
public:
    FFTData()
        : amp(new double[N1])
    {
        out = fftw_alloc_complex(HALF_N);
        in = (double*)out;
        plan = fftw_plan_dft_r2c_1d(N, in, out, FFTW_MEASURE);
        memset(out, 0, HALF_N * sizeof(fftw_complex));
    }

    ~FFTData()
    {
        fftw_destroy_plan(plan);
        fftw_free(out);
        delete[] amp;
    }

    void read(SlidingWindow<double>& window) { window.read(in, N); }

    void execute() { fftw_execute(plan); }

public:
    fftw_complex* out;
    double* amp;

private:
    fftw_plan plan;
    double* in;
};

class Spectrum {
public:
    Spectrum(audio_data* data)
        : audio(data)
        , freq(new freq_data[N1])
    {
        precalc();
    }

    ~Spectrum() { delete[] freq; }

    void precalc();
    void process();

public:
    audio_data* audio;
    freq_data* freq;
    FFTData left, right;
    int minK, maxK;
};

// "Saw" function with specified range
inline double saw(double val, double p)
{
    double x = val / p;
    return p * fabs(x - floor(x + 0.5));
}

void Spectrum::precalc()
{
    double maxFreq = N;
    double minFreq = audio->rate / maxFreq;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    // Notes in [36, 108] range, i.e. 6 octaves
    minK = ceil(exp(35 * base) / (minFreq * fcoef));
    maxK = ceil(exp(108 * base) / (minFreq * fcoef));

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

        freq[k] = f;
    }
}

void Spectrum::process()
{
    left.read(audio->left);
    if (audio->channels == 2)
        right.read(audio->right);

    left.execute();
    if (audio->channels == 2)
        right.execute();

    fftw_complex* outl = left.out;
    fftw_complex* outr = right.out;
    int maxF = minK;
    double maxAmp = 0;

    for (int k = minK; k < maxK; k++) {
        double ampl = left.amp[k] = hypot(outl[k][0], outl[k][1]);
        double ampr = right.amp[k] = hypot(outr[k][0], outr[k][1]);
        double amp = std::max(ampl, ampr);
        if (amp > maxAmp) {
            maxAmp = amp;
            maxF = k;
        }
    }

    audio->curColor = freq[maxF].c;
}

void redraw(Spectrum& spec, video_data& video)
{
    Display* dis = video.dis;
    if (dis == nullptr)
        return;

    Window win = video.win;
    GC gc = video.gc;
    Pixmap double_buffer = video.double_buffer;

    while (XPending(dis) > 0) {
        XEvent event;
        XNextEvent(dis, &event);
        if ((event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_q)
            || (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == video.close)) {
            spec.audio->terminate = true;
            return;
        }
    }

    unsigned int width, height, bw, dr;
    Window root;
    int xx, yy;
    XGetGeometry(dis, win, &root, &xx, &yy, &width, &height, &bw, &dr);

    freq_data* freq = spec.freq;
    double minNote = 34;
    double maxNote = 110;
    double kx = width / (maxNote - minNote);
    double ky = height * 0.25 / 65536.0;

    if (width != video.last_width || height != video.last_height) {
        video.last_width = width;
        video.last_height = height;
        if (double_buffer != 0) {
            XFreePixmap(dis, double_buffer);
        }
        double_buffer = XCreatePixmap(dis, win, width, height, 24);
        video.double_buffer = double_buffer;

        for (int k = 1; k < N1; k++) {
            freq[k].x = (int)((freq[k].note - minNote) * kx + 0.5);
        }
    }

    XSetForeground(dis, gc, 0);
    XFillRectangle(dis, double_buffer, gc, 0, 0, width, height);

    int lastx = -1;
    double prevAmpL = 0;
    double prevAmpR = 0;
    int minK = spec.minK, maxK = spec.maxK;

    for (int k = minK; k < maxK; k++) {
        prevAmpL = std::max(prevAmpL, spec.left.amp[k]);
        prevAmpR = std::max(prevAmpR, spec.right.amp[k]);
        int x = freq[k].x;
        if (lastx < x) {
            lastx = x + 3;
            int yl = (int)(prevAmpL * ky + 0.5);
            int yr = (int)(prevAmpR * ky + 0.5);
            prevAmpL = 0;
            prevAmpR = 0;
            XSetForeground(dis, gc, freq[k].ic);
            XDrawLine(dis, double_buffer, gc, x, height * 0.5 - yl, x, height * 0.5 + yr);
        }
    }

    XCopyArea(dis, double_buffer, win, gc, 0, 0, width, height, 0, 0);
    XFlush(dis);
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
    init_x(video);

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
        redraw(spec, video);
    }

    pthread_join(input_thread, NULL);
    for (espurna& strip : strips)
        pthread_join(strip.p_thread, NULL);

    close_x(video);

    return 0;
}
