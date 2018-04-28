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

#define max(a, b)                                                                                                      \
    ({                                                                                                                 \
        __typeof__(a) _a = (a);                                                                                        \
        __typeof__(b) _b = (b);                                                                                        \
        _a > _b ? _a : _b;                                                                                             \
    })

#ifdef __GNUC__
// curses.h or other sources may already define
#undef GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

int print_raw_out(int bars_count, int fd, int ascii_range, char bar_delim, char frame_delim, const int const f[200])
{
    for (int i = 0; i < bars_count; i++) {
        int f_ranged = f[i];
        if (f_ranged > ascii_range)
            f_ranged = ascii_range;

        // finding size of number-string in byte
        int bar_height_size = 2; // a number + \0
        if (f_ranged != 0)
            bar_height_size += floor(log10(f_ranged));

        char bar_height[bar_height_size];
        snprintf(bar_height, bar_height_size, "%d", f_ranged);

        write(fd, bar_height, bar_height_size - 1);
        write(fd, &bar_delim, sizeof(bar_delim));
    }
    write(fd, &frame_delim, sizeof(frame_delim));
    return 0;
}

double smoothDef[64] = { 0.8, 0.8, 1, 1, 0.8, 0.8, 1, 0.8, 0.8, 1, 1, 0.8, 1, 1, 0.8, 0.6, 0.6, 0.7, 0.8, 0.8, 0.8, 0.8,
    0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
    0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6 };

struct config_params {
    char *color, *bcolor, *raw_target, *audio_source, *gradient_color_1, *gradient_color_2, *data_format;
    char bar_delim, frame_delim;
    double monstercat, integral, gravity, ignore, sens;
    unsigned int lowcf, highcf;
    double* smooth;
    int smcount, customEQ, col, bgcol, stereo, is_bin, ascii_range, bit_format, gradient, fixedbars,
        framerate, bw, bs, autosens, overshoot, waves;
};

struct audio_data {
    int audio_out_r[2048];
    int audio_out_l[2048];
    int format;
    unsigned int rate;
    char* source; // alsa device
    int channels;
    int terminate; // shared variable used to terminate audio thread
    char error_message[1024];
};

// assuming stereo
#define CHANNELS_COUNT 2
#define SAMPLE_RATE 44100

static void initialize_audio_parameters(snd_pcm_t** handle, struct audio_data* audio, snd_pcm_uframes_t* frames)
{
    // alsa: open device to capture audio
    int err = snd_pcm_open(handle, audio->source, SND_PCM_STREAM_CAPTURE, 0);
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
    // number of frames pr read
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

static int get_certain_frame(signed char* buffer, int buffer_index, int adjustment)
{
    // using the 10 upper bits this would give me a vert res of 1024, enough...
    int temp = buffer[buffer_index + adjustment - 1] << 2;
    int lo = buffer[buffer_index + adjustment - 2] >> 6;
    if (lo < 0)
        lo = abs(lo) + 1;
    if (temp >= 0)
        temp += lo;
    else
        temp -= lo;
    return temp;
}

static void fill_audio_outs(struct audio_data* audio, signed char* buffer, const int size)
{
    int radj = audio->format / 4; // adjustments for interleaved
    int ladj = audio->format / 8;
    static int audio_out_buffer_index = 0;
    // sorting out one channel and only biggest octet
    for (int buffer_index = 0; buffer_index < size; buffer_index += ladj * 2) {
        // first channel
        int tempr = get_certain_frame(buffer, buffer_index, radj);
        // second channel
        int templ = get_certain_frame(buffer, buffer_index, ladj);

        // mono: adding channels and storing it in the buffer
        if (audio->channels == 1)
            audio->audio_out_l[audio_out_buffer_index] = (templ + tempr) / 2;
        else { // stereo storing channels in buffer
            audio->audio_out_l[audio_out_buffer_index] = templ;
            audio->audio_out_r[audio_out_buffer_index] = tempr;
        }

        ++audio_out_buffer_index;
        audio_out_buffer_index %= 2048;
    }
}

#define FRAMES_NUMBER 256

void* input_alsa(void* data)
{
    int err;
    struct audio_data* audio = (struct audio_data*)data;
    snd_pcm_t* handle;
    snd_pcm_uframes_t frames = FRAMES_NUMBER;
    int16_t buf[FRAMES_NUMBER * 2];
    initialize_audio_parameters(&handle, audio, &frames);
    // frames * bits/8 * channels
    const int size = frames * (audio->format / 8) * CHANNELS_COUNT;
    signed char* buffer = malloc(size);
    int n = 0;

    while (1) {
        switch (audio->format) {
        case 16:
            err = snd_pcm_readi(handle, buf, frames);
            for (int i = 0; i < FRAMES_NUMBER * 2; i += 2) {
                if (audio->channels == 1)
                    audio->audio_out_l[n] = (buf[i] + buf[i + 1]) / 2;
                // stereo storing channels in buffer
                if (audio->channels == 2) {
                    audio->audio_out_l[n] = buf[i];
                    audio->audio_out_r[n] = buf[i + 1];
                }
                n++;
                if (n == 2048 - 1)
                    n = 0;
            }
            break;
        default:
            err = snd_pcm_readi(handle, buffer, frames);
            fill_audio_outs(audio, buffer, size);
            break;
        }

        if (err == -EPIPE) {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(err));
        } else if (err != (int)frames) {
            fprintf(stderr, "short read, read %d %d frames\n", err, (int)frames);
        }

        if (audio->terminate == 1) {
            free(buffer);
            snd_pcm_close(handle);
            return NULL;
        }
    }
}

int M = 2048;

// general: handle signals
void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
    }
    signal(sig_no, SIG_DFL);
    raise(sig_no);
}

int* separate_freq_bands(fftw_complex out[M / 2 + 1][2], int bars, int lcf[200], int hcf[200], float k[200],
    int channel, double sens, double ignore)
{
    int o, i;
    float peak[201];
    static int fl[200];
    static int fr[200];
    int y[M / 2 + 1];
    float temp;

    // process: separate frequency bands
    for (o = 0; o < bars; o++) {
        peak[o] = 0;

        // process: get peaks
        for (i = lcf[o]; i <= hcf[o]; i++) {
            // getting r of compex
            y[i] = pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5);
            peak[o] += y[i]; // adding upp band
        }

        peak[o] = peak[o] / (hcf[o] - lcf[o] + 1); // getting average
        temp = peak[o] * k[o] * sens; // multiplying with k and adjusting to sens settings
        if (temp <= ignore)
            temp = 0;
        if (channel == 1)
            fl[o] = temp;
        else
            fr[o] = temp;
    }

    if (channel == 1)
        return fl;
    else
        return fr;
}

int* monstercat_filter(int* f, int bars, int waves, double monstercat)
{
    int z;

    // process [smoothing]: monstercat-style "average"

    int m_y, de;
    if (waves > 0) {
        for (z = 0; z < bars; z++) { // waves
            f[z] = f[z] / 1.25;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                f[m_y] = max(f[z] - pow(de, 2), f[m_y]);
            }
            for (m_y = z + 1; m_y < bars; m_y++) {
                de = m_y - z;
                f[m_y] = max(f[z] - pow(de, 2), f[m_y]);
            }
        }
    } else if (monstercat > 0) {
        for (z = 0; z < bars; z++) {
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                f[m_y] = max(f[z] / pow(monstercat, de), f[m_y]);
            }
            for (m_y = z + 1; m_y < bars; m_y++) {
                de = m_y - z;
                f[m_y] = max(f[z] / pow(monstercat, de), f[m_y]);
            }
        }
    }

    return f;
}

// general: entry point
int main()
{
    // general: define variables
    pthread_t p_thread;
    int thr_id GCC_UNUSED;
    float fc[200];
    float fre[200];
    int f[200], lcf[200], hcf[200];
    int *fl, *fr;
    int fmem[200];
    int flast[200];
    int sleep = 0;
    int i, n, o, height, w, rest, silence, fp;
    int fall[200];
    float fpeak[200];
    float k[200];
    float g;
    struct timespec req = {.tv_sec = 0, .tv_nsec = 0 };

    double inr[2 * (M / 2 + 1)];
    double inl[2 * (M / 2 + 1)];
    int bars = 25;
    double smh;

    struct audio_data audio;

    struct config_params p = {
        .monstercat = 1.5,
        .waves = 0,
        .integral = 0.9f,
        .gravity = 1,
        .ignore = 0,
        .color = "default",
        .bcolor = "default",
        .gradient = 0,
        .fixedbars = 48,
        .bw = 2,
        .bs = 1,
        .framerate = 60,
        .sens = 1,
        .autosens = 1,
        .overshoot = 20,
        .lowcf = 50,
        .highcf = 10000,
        .raw_target = "/dev/stdout",
        .data_format = "ascii",
        .bar_delim = 32,
        .frame_delim = 10,
        .ascii_range = 1000,
        .bit_format = 16,
        .smooth = smoothDef,
        .customEQ = 0,
        .smcount = 64, // back to the default one
        .audio_source = "hw:CARD=sndrpigooglevoi,DEV=0",
        .is_bin = 0,
        .stereo = 1,
    };

    // general: handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    // fft: planning to rock
    fftw_complex outl[M / 2 + 1][2];
    fftw_plan pl = fftw_plan_dft_r2c_1d(M, inl, *outl, FFTW_MEASURE);

    fftw_complex outr[M / 2 + 1][2];
    fftw_plan pr = fftw_plan_dft_r2c_1d(M, inr, *outr, FFTW_MEASURE);

    // input: init
    audio.source = p.audio_source;
    audio.format = -1;
    audio.rate = 0;
    audio.terminate = 0;
    audio.channels = (p.stereo ? 2 : 1);

    for (i = 0; i < M; i++) {
        audio.audio_out_l[i] = 0;
        audio.audio_out_r[i] = 0;
    }

    thr_id = pthread_create(&p_thread, NULL, input_alsa, (void*)&audio); // starting alsamusic listener

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

    if (p.highcf > audio.rate / 2) {
        fprintf(stderr, "higher cuttoff frequency can't be higher then "
                        "sample rate / 2");
        exit(EXIT_FAILURE);
    }

    bool senseLow = 1;

    for (i = 0; i < 200; i++) {
        flast[i] = 0;
        fall[i] = 0;
        fpeak[i] = 0;
        fmem[i] = 0;
        f[i] = 0;
    }

    fp = open(p.raw_target, O_WRONLY | O_NONBLOCK | O_CREAT, 0644);
    if (fp == -1) {
        printf("could not open file %s for writing\n", p.raw_target);
        exit(1);
    }
    printf("open file %s for writing raw ouput\n", p.raw_target);

    // width must be hardcoded for raw output.
    w = 200;

    height = p.ascii_range;

    // getting orignial numbers of barss incase of resize
    bars = p.fixedbars;

    if (bars < 1)
        bars = 1; // must have at least 1 bars
    if (bars > 200)
        bars = 200; // cant have more than 200 bars

    if (p.stereo) { // stereo must have even numbers of bars
        if (bars % 2 != 0)
            bars--;
    }

    // process [smoothing]: calculate gravity
    g = p.gravity * ((float)height / 2160) * pow((60 / (float)p.framerate), 2.5);

    // checks if there is stil extra room, will use this to center
    rest = (w - bars * p.bw - bars * p.bs + p.bs) / 2;
    if (rest < 0)
        rest = 0;

    if (p.stereo)
        bars = bars / 2; // in stereo onle half number of bars per channel

    if ((p.smcount > 0) && (bars > 0)) {
        smh = (double)(((double)p.smcount) / ((double)bars));
    }

    double freqconst = log10((float)p.lowcf / (float)p.highcf) / ((float)1 / ((float)bars + (float)1) - 1);

    // process: calculate cutoff frequencies
    for (n = 0; n < bars + 1; n++) {
        fc[n] = p.highcf * pow(10, freqconst * (-1) + ((((float)n + 1) / ((float)bars + 1)) * freqconst));
        fre[n] = fc[n] / (audio.rate / 2);
        // remember nyquist!, pr my calculations this should be rate/2
        // and nyquist freq in M/2 but testing shows it is not...
        // or maybe the nq freq is in M/4

        // lfc stores the lower cut frequency foo each bar in the fft out buffer
        lcf[n] = fre[n] * (M / 4);
        if (n != 0) {
            hcf[n - 1] = lcf[n] - 1;

            // pushing the spectrum up if the expe function gets "clumped"
            if (lcf[n] <= lcf[n - 1])
                lcf[n] = lcf[n - 1] + 1;
            hcf[n - 1] = lcf[n] - 1;
        }
    }

    // process: weigh signal to frequencies
    for (n = 0; n < bars; n++)
        k[n] = pow(fc[n], 0.85) * ((float)height / (M * 32000)) * p.smooth[(int)floor(((double)n) * smh)];

    if (p.stereo)
        bars = bars * 2;

    while (true) {
        // process: populate input buffer and check if input is present
        silence = 1;
        for (i = 0; i < (2 * (M / 2 + 1)); i++) {
            if (i < M) {
                inl[i] = audio.audio_out_l[i];
                if (p.stereo)
                    inr[i] = audio.audio_out_r[i];
                if (inl[i] || inr[i])
                    silence = 0;
            } else {
                inl[i] = 0;
                if (p.stereo)
                    inr[i] = 0;
            }
        }

        if (silence == 1)
            sleep++;
        else
            sleep = 0;

        // process: if input was present for the last 5 seconds apply FFT to it
        if (sleep < p.framerate * 5) {
            // process: execute FFT and sort frequency bands
            if (p.stereo) {
                fftw_execute(pl);
                fftw_execute(pr);

                fl = separate_freq_bands(outl, bars / 2, lcf, hcf, k, 1, p.sens, p.ignore);
                fr = separate_freq_bands(outr, bars / 2, lcf, hcf, k, 2, p.sens, p.ignore);
            } else {
                fftw_execute(pl);
                fl = separate_freq_bands(outl, bars, lcf, hcf, k, 1, p.sens, p.ignore);
            }
        } else { //**if in sleep mode wait and continue**//
            // wait 1 sec, then check sound again.
            req.tv_sec = 1;
            req.tv_nsec = 0;
            nanosleep(&req, NULL);
            continue;
        }

        // process [smoothing]

        if (p.monstercat) {
            if (p.stereo) {
                fl = monstercat_filter(fl, bars / 2, p.waves, p.monstercat);
                fr = monstercat_filter(fr, bars / 2, p.waves, p.monstercat);
            } else {
                fl = monstercat_filter(fl, bars, p.waves, p.monstercat);
            }
        }

        // preperaing signal for drawing
        for (o = 0; o < bars; o++) {
            if (p.stereo) {
                if (o < bars / 2) {
                    f[o] = fl[bars / 2 - o - 1];
                } else {
                    f[o] = fr[o - bars / 2];
                }
            } else {
                f[o] = fl[o];
            }
        }

        // process [smoothing]: falloff
        if (g > 0) {
            for (o = 0; o < bars; o++) {
                if (f[o] < flast[o]) {
                    f[o] = fpeak[o] - (g * fall[o] * fall[o]);
                    fall[o]++;
                } else {
                    fpeak[o] = f[o];
                    fall[o] = 0;
                }

                flast[o] = f[o];
            }
        }

        // process [smoothing]: integral
        if (p.integral > 0) {
            for (o = 0; o < bars; o++) {
                f[o] = fmem[o] * p.integral + f[o];
                fmem[o] = f[o];

                int diff = (height + 1) - f[o];
                if (diff < 0)
                    diff = 0;
                double div = 1 / (diff + 1);
                fmem[o] = fmem[o] * (1 - div / 20);
            }
        }

        // zero values causes divided by zero segfault
        for (o = 0; o < bars; o++) {
            if (f[o] < 1) {
                f[o] = 1;
            }
        }

        // autmatic sens adjustment
        if (p.autosens) {
            for (o = 0; o < bars; o++) {
                if (f[o] > height) {
                    senseLow = 0;
                    p.sens = p.sens * 0.985;
                    break;
                }
                if (senseLow && !silence)
                    p.sens = p.sens * 1.01;
                if (o == bars - 1)
                    p.sens = p.sens * 1.002;
            }
        }

        // output: draw processed input
        print_raw_out(bars, fp, p.ascii_range, p.bar_delim, p.frame_delim, f);

        if (p.framerate <= 1) {
            req.tv_sec = 1 / (float)p.framerate;
        } else {
            req.tv_sec = 0;
            req.tv_nsec = (1 / (float)p.framerate) * 1000000000;
        }

        nanosleep(&req, NULL);

        // checking if audio thread has exited unexpectedly
        if (audio.terminate == 1) {
            fprintf(stderr, "Audio thread exited unexpectedly. %s\n", audio.error_message);
            break;
        }
    }

    req.tv_sec = 0;
    req.tv_nsec = 100; // waiting some time to make sure audio is ready
    nanosleep(&req, NULL);

    //**telling audio thread to terminate**//
    audio.terminate = 1;
    pthread_join(p_thread, NULL);

    return 0;
}
