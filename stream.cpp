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

    win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 640, 360, 5, black, white);
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

/*
private void doDrawHiFiFFT(Canvas canvas)
{
    int k, fbase = mCaptureSize - mCaptureUsed;
    for (k = 0; k < mCaptureUsed; k++) {
        re[k] = (left[fbase + k] + right[fbase + k]) * 0.5;
        im[k] = 0;
    }

    fftCalc.fft(re, im);

    if (drawMode == 1)
        canvas.drawRGB(0, 0, 0);
    Paint p = new Paint();

    int maxFreq = mCaptureUsed;
    double base = Math.log(Math.pow(2, 1.0 / 12.0));

    double fcoef = Math.pow(2, 57.0 / 12.0) / 440.0; // Frequency 440 is a note number 57 = 12 * 4 + 9
    double minFreq = mSamplingRate / maxFreq;
    double minNote = Math.log(minFreq * fcoef) / base;
    double minOctave = Math.floor(minNote / 12.0);
    fcoef = Math.pow(2, ((4 - minOctave) * 12 + 9) / 12.0) / 440.0; // Shift everything by several octaves
    double maxNote = Math.log(mSamplingRate * fcoef) / base;

    int baseY = (mCanvasHeight * 3) / 4;

    double kx = mCanvasWidth / maxNote;
    double ky = mCanvasHeight * 0.5;
    int lastx = -1;
    int maxY = 0;

    double maxAmp = 0;
    int maxR = 0, maxG = 0, maxB = 0;
    int cRing = 0;
    double peakN = 0, maxN = 0;

    for (k = 1; k < maxFreq >> 1; k++) {
        double amp = Math.hypot(re[k], im[k]) / 256.0;
        double frequency = (k * (double) mSamplingRate) / maxFreq;
        double note = Math.log(frequency * fcoef) / base; // note = 12 * Octave + Note
        maxN = note;

        if (drawMode == 1) {
            int x = (int) Math.round(note * kx);
            int y = (int) Math.round(amp * ky);

            if (y > maxY)
                maxY = y;

            if (lastx != x) {
                double spectre = note - 12.0 * Math.floor(note / 12.0); // spectre is within [0, 12)

                lastx = x;
                maxY = 0;

                double R = clamp(spectre - 6);
                double G = clamp(spectre - 10);
                double B = clamp(spectre - 2);

                double mx = Math.max(Math.max(R, G), B);
                double mn = Math.min(Math.min(R, G), B);
                double mm = mx - mn;
                if (mm == 0) mm = 1;

                R = (R - mn) / mm;
                G = (G - mn) / mm;
                B = (B - mn) / mm;

                p.setARGB(255, (int) Math.round(R * 255), (int) Math.round(G * 255), (int) Math.round(B * 255));
                canvas.drawLine(x, baseY, x, baseY - y, p);
            }
        }
        else if (amp > maxAmp) {
            maxAmp = amp;
            double spectre = note - 12.0 * Math.floor(note / 12.0); // spectre is within [0, 12)

            int ring = (96 - (int) Math.floor(spectre * 8)); // [1 .. 96]
            cRing = ring + 48;
            if (cRing > 100)
                cRing -= 100; // [1 .. 100]

            double R = clamp(spectre - 6);
            double G = clamp(spectre - 10);
            double B = clamp(spectre - 2);

            double mx = Math.max(Math.max(R, G), B);
            double mn = Math.min(Math.min(R, G), B);
            double mm = mx - mn;
            if (mm == 0) mm = 1;

            maxR = (int) Math.round(255.0 * (R - mn) / mm);
            maxG = (int) Math.round(255.0 * (G - mn) / mm);
            maxB = (int) Math.round(255.0 * (B - mn) / mm);
            peakN = note;
        }
    }
    if (drawMode != 1)
        canvas.drawRGB(maxR, maxG, maxB);
    if (cRing != prevColor)
        setColor(cRing);
    prevColor = cRing;
}
*/

// assuming stereo
#define CHANNELS_COUNT 2
#define SAMPLE_RATE 44100

const char* audio_source = "hw:CARD=sndrpigooglevoi,DEV=0";
constexpr int M = 2048;
constexpr int HALF_M = M / 2 + 1;
constexpr int M2 = 2 * HALF_M;

struct audio_data {
    int format;
    unsigned int rate;
    int channels;
    bool terminate; // shared variable used to terminate audio thread
    char error_message[1024];

    SlidingWindow<double> left{ 65536 };
    SlidingWindow<double> right{ 65536 };
};

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

            int16_t* left = (int16_t*)(buffer + loff);
            int16_t* right = (int16_t*)(buffer + roff);
            for (unsigned int i = 0; i < frames; i++) {
                left_data[i] = *left;
                right_data[i] = *right;
                left += stride;
                right += stride;
            }

            audio->left.write(left_data, frames);
            audio->right.write(right_data, frames);
        }
    }

    snd_pcm_close(handle);
    delete[] buffer;

    return NULL;
}

void redraw(struct audio_data* audio)
{
    if (dis == nullptr)
        return;
    XClearWindow(dis, win);

    int last_x = 0;
    int last_y = 180;

    double data[640 * 64];
    audio->left.read(data, 640 * 64);

    for (int i = 0; i < 640 * 64; i += 64) {
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
}

// general: handle signals
void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
    }
    signal(sig_no, SIG_DFL);
    raise(sig_no);
}

// general: entry point
int main()
{
    // general: define variables
    pthread_t p_thread;
    int sleep = 0;
    int i, n;
    double inl[M2], inr[M2];
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
    fftw_complex outl[HALF_M];
    fftw_plan pl = fftw_plan_dft_r2c_1d(M, inl, outl, FFTW_MEASURE);

    fftw_complex outr[HALF_M];
    fftw_plan pr = fftw_plan_dft_r2c_1d(M, inr, outr, FFTW_MEASURE);

    // input: init
    audio.format = -1;
    audio.rate = 0;
    audio.terminate = false;
    audio.channels = (stereo ? 2 : 1);

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

    while (!audio.terminate) {
        // process: populate input buffer and check if input is present
        bool silence = true;

        audio.left.read(inl, M);
        if (stereo)
            audio.right.read(inr, M);

        for (i = 0; i < M; i++) {
            if (inl[i] || (stereo && inr[i]))
                silence = false;
        }

        for (i = M; i < M2; i++) {
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

        req.tv_sec = 0;
        req.tv_nsec = 1e9 / framerate;
        nanosleep(&req, NULL);

        while (dis != nullptr && XPending(dis) > 0) {
            XNextEvent(dis, &event);

            if (event.type == KeyPress && XLookupString(&event.xkey, text, 255, &key, 0) == 1) {
                if (text[0] == 'q')
                    audio.terminate = true;
            }
        }

        redraw(&audio);
    }

    pthread_join(p_thread, NULL);
    close_x();
    fftw_destroy_plan(pl);
    fftw_destroy_plan(pr);

    return 0;
}
