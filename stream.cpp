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

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <stdio.h>

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

    win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 648, 360, 5, black, white);
    XSetStandardProperties(dis, win, "My Window", "HI!", None, NULL, 0, NULL);
    XSelectInput(dis, win, ExposureMask | ButtonPressMask | KeyPressMask);

    gc = XCreateGC(dis, win, 0, 0);

    XClearWindow(dis, win);
    XMapRaised(dis, win);
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

const char* audio_source = "hw:CARD=sndrpigooglevoi,DEV=0";
constexpr int M = 14;
constexpr int N = 1 << M;
constexpr int N1 = N / 2;
constexpr int HALF_N = N / 2 + 1;
constexpr int N2 = 2 * HALF_N;

struct audio_data {
    int format;
    unsigned int rate;
    int channels;
    bool terminate; // shared variable used to terminate audio thread

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };
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
            case 32: {
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
                    left_data[i] = *left;
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

double clamp(double val)
{
    if (val < 0)
        val += 12;
    if (val > 6)
        val = 12 - val;
    return val / 6.0;
}

void redraw(struct audio_data* audio __attribute__((unused)), fftw_complex* out)
{
    if (dis != nullptr)
        XClearWindow(dis, win);

    /*
    // Wave drawing
    int last_x = 0;
    int last_y = 180;
    double data[648 * 64];

    audio->left.read(data, 648 * 64);

    for (int i = 0; i < 648 * 64; i += 64) {
        int x = i / 64;
        int avg = 0;
        for (int k = 0; k < 64; k++) {
            avg += data[i];
        }
        avg /= 64;
        int y = 180 + avg;

        XDrawLine(dis, win, gc, last_x, last_y, x, y);

        last_x = x;
        last_y = y;
    }
    */

    int width = 648, height = 360;
    double base = log(pow(2, 1.0 / 12.0));
    double fcoef = pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9

    double maxFreq = N;
    double minFreq = SAMPLE_RATE / maxFreq;
    // double minNote = log(minFreq * fcoef) / base;
    // double minOctave = floor(minNote / 12.0);
    // fcoef = pow(2, ((4 - minOctave) * 12 + 9) / 12.0) / 440.0; // Shift everything by several octaves
    // double maxNote = log(SAMPLE_RATE * fcoef) / base;

    int curNote = 11;
    double nextFreq = pow(2, (curNote + 0.5) / 12.0) / fcoef;
    double prevNote = 0;
    double prevAmp = 0;
    double integ = 0;

    int baseY = (height * 3) / 4;

    double kx = width / 108.0; // 12 * 9
    double ky = height * 0.5 / 65536.0;
    int lastx = -1;

    double maxAmp = 0;
    int maxR = 0, maxG = 0, maxB = 0;

    for (int k = 1; k < N1 && curNote < 120; k++) {
        double frequency = k * minFreq;
        double note = log(frequency * fcoef) / base; // note = 12 * Octave + Note
        double amp = hypot(out[k][0], out[k][1]);

        if (note > curNote + 0.5) {
            amp = prevAmp + (amp - prevAmp) * ((curNote + 0.5) - prevNote) / (note - prevNote);
            note = curNote + 0.5;
            frequency = nextFreq;
        }

        integ += (note - prevNote) * (prevAmp + amp) / 2;

        if (frequency == nextFreq) {
            if (curNote >= 12) {
                double R = clamp(curNote - 6);
                double G = clamp(curNote - 10);
                double B = clamp(curNote - 2);

                if (integ > maxAmp) {
                    maxAmp = integ;

                    double mx = std::max(std::max(R, G), B);
                    double mn = std::min(std::min(R, G), B);
                    double mm = mx - mn;
                    if (mm == 0)
                        mm = 1;

                    maxR = (int)floor(255.0 * (R - mn) / mm + 0.5);
                    maxG = (int)floor(255.0 * (G - mn) / mm + 0.5);
                    maxB = (int)floor(255.0 * (B - mn) / mm + 0.5);
                }

                if (dis != nullptr) {
                    int x1 = (int)floor((curNote - 12) * kx + 0.5);
                    int x2 = (int)floor((curNote - 11) * kx + 0.5);
                    int y = (int)floor(integ * ky + 0.5);
                    XDrawLine(dis, win, gc, x1, baseY - y, x2, baseY - y);
                }
            }

            curNote++;
            integ = 0;
            nextFreq = pow(2, (curNote + 0.5) / 12.0) / fcoef;
            k--;
        } else if (dis != nullptr && note >= 11.5) {
            // note is within [curNote - 0.5, curNote + 0.5)
            int x = (int)floor((note - 11.5) * kx + 0.5);
            if (lastx != x) {
                /*
                double spectre = note - 12.0 * floor(note / 12.0); // spectre is within [0, 12)

                lastx = x;

                double R = clamp(spectre - 6);
                double G = clamp(spectre - 10);
                double B = clamp(spectre - 2);

                double mx = max(max(R, G), B);
                double mn = min(min(R, G), B);
                double mm = mx - mn;
                if (mm == 0) mm = 1;

                R = (R - mn) / mm;
                G = (G - mn) / mm;
                B = (B - mn) / mm;

                p.setARGB(255, (int) round(R * 255), (int) round(G * 255), (int) round(B * 255));
                */
                int y = (int)floor(amp * ky + 0.5);
                XDrawLine(dis, win, gc, x, baseY, x, baseY - y);
            }
        }

        prevNote = note;
        prevAmp = amp;
    }

    std::cout << maxR << "," << maxG << "," << maxB << std::endl;
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
int main()
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

    // input: init
    audio.format = -1;
    audio.rate = 0;
    audio.terminate = false;
    audio.channels = (stereo ? 2 : 1);

    g_audio = &audio;
    pthread_create(&p_thread, NULL, input_alsa, (void*)&audio); // starting alsamusic listener

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

    long frame_time = 1e9 / framerate;

    while (!audio.terminate) {
        auto start = std::chrono::high_resolution_clock::now();

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

            if (event.type == KeyPress && XLookupString(&event.xkey, text, 255, &key, 0) == 1) {
                if (text[0] == 'q')
                    audio.terminate = true;
            }
        }

        redraw(&audio, outl);

        auto finish = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
        // std::cout << duration << " ns\n";

        if (duration < frame_time) {
            req.tv_sec = 0;
            req.tv_nsec = frame_time - duration;
            // std::cout << "sleep for " << req.tv_nsec << " ns\n";
            nanosleep(&req, NULL);
        }
    }

    pthread_join(p_thread, NULL);
    close_x();
    fftw_destroy_plan(pl);
    fftw_destroy_plan(pr);

    return 0;
}
