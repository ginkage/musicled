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

double smoothDef[64] = { 0.8, 0.8, 1, 1, 0.8, 0.8, 1, 0.8, 0.8, 1, 1, 0.8, 1, 1, 0.8, 0.6, 0.6, 0.7, 0.8, 0.8, 0.8, 0.8,
    0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
    0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6 };

struct config_params {
};

struct audio_data {
    int audio_out_r[2048];
    int audio_out_l[2048];
    int format;
    unsigned int rate;
    int channels;
    bool terminate; // shared variable used to terminate audio thread
    char error_message[1024];
};

// assuming stereo
#define CHANNELS_COUNT 2
#define SAMPLE_RATE 44100

constexpr char* audio_source = "hw:CARD=sndrpigooglevoi,DEV=0";

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
    signed char* buffer = new signed char[size];
    int n = 0;

    while (!audio->terminate) {
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
    }

    delete[] buffer;
    snd_pcm_close(handle);

    return NULL;
}

constexpr int M = 2048;
constexpr int HALF_M = M / 2 + 1;
constexpr int M2 = 2 * HALF_M;

// general: handle signals
void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
        audio.terminate = true;
    }
    signal(sig_no, SIG_DFL);
    raise(sig_no);
}

int* separate_freq_bands(fftw_complex out[HALF_M][2], int bars, int lcf[200], int hcf[200], float k[200], int channel,
    double sens, double ignore)
{
    int o, i;
    float peak[201];
    static int fl[200];
    static int fr[200];
    int y[HALF_M];
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
                f[m_y] = std::max(f[m_y], int(f[z] - pow(de, 2)));
            }
            for (m_y = z + 1; m_y < bars; m_y++) {
                de = m_y - z;
                f[m_y] = std::max(f[m_y], int(f[z] - pow(de, 2)));
            }
        }
    } else if (monstercat > 0) {
        for (z = 0; z < bars; z++) {
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                f[m_y] = std::max(f[m_y], int(f[z] / pow(monstercat, de)));
            }
            for (m_y = z + 1; m_y < bars; m_y++) {
                de = m_y - z;
                f[m_y] = std::max(f[m_y], int(f[z] / pow(monstercat, de)));
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
    float fc[200];
    float fre[200];
    int f[200], lcf[200], hcf[200];
    int *fl, *fr;
    int fmem[200];
    int flast[200];
    int sleep = 0;
    int i, n, o;
    int fall[200];
    float fpeak[200];
    float k[200];
    float g;
    struct timespec req = {.tv_sec = 0, .tv_nsec = 0 };

    double inr[M2];
    double inl[M2];

    struct audio_data audio;

    double monstercat = 1.5, integral = 0.9, gravity = 1.0, ignore = 0.0, sens = 1.0;
    unsigned int lowcf = 50, highcf = 10000;
    double* smooth = smoothDef;
    int smcount = 64, height = 1000, bars = 48, framerate = 60;
    bool stereo = true, autosens = true, waves = false;
    bool senseLow = true;

    // general: handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);

    // fft: planning to rock
    fftw_complex outl[HALF_M][2];
    fftw_plan pl = fftw_plan_dft_r2c_1d(M, inl, *outl, FFTW_MEASURE);

    fftw_complex outr[HALF_M][2];
    fftw_plan pr = fftw_plan_dft_r2c_1d(M, inr, *outr, FFTW_MEASURE);

    // input: init
    audio.format = -1;
    audio.rate = 0;
    audio.terminate = false;
    audio.channels = (stereo ? 2 : 1);

    for (i = 0; i < M; i++) {
        audio.audio_out_l[i] = 0;
        audio.audio_out_r[i] = 0;
    }

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

    if (highcf > audio.rate / 2) {
        fprintf(stderr, "higher cuttoff frequency can't be higher then "
                        "sample rate / 2");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < 200; i++) {
        flast[i] = 0;
        fall[i] = 0;
        fpeak[i] = 0;
        fmem[i] = 0;
        f[i] = 0;
    }

    // process [smoothing]: calculate gravity
    g = gravity * ((float)height / 2160) * pow((60 / (float)framerate), 2.5);

    if (stereo)
        bars = bars / 2; // in stereo onle half number of bars per channel

    double freqconst = log10((float)lowcf / (float)highcf) / ((float)1 / ((float)bars + (float)1) - 1);

    // process: calculate cutoff frequencies
    for (n = 0; n < bars + 1; n++) {
        fc[n] = highcf * pow(10, freqconst * (-1) + ((((float)n + 1) / ((float)bars + 1)) * freqconst));
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
    double smh = (double)(((double)smcount) / ((double)bars));
    for (n = 0; n < bars; n++)
        k[n] = pow(fc[n], 0.85) * ((float)height / (M * 32000)) * smooth[(int)floor(((double)n) * smh)];

    if (stereo)
        bars = bars * 2;

    while (!audio.terminate) {
        // process: populate input buffer and check if input is present
        bool silence = true;
        for (i = 0; i < M2; i++) {
            if (i < M) {
                inl[i] = audio.audio_out_l[i];
                if (stereo)
                    inr[i] = audio.audio_out_r[i];
                if (inl[i] || inr[i])
                    silence = false;
            } else {
                inl[i] = 0;
                if (stereo)
                    inr[i] = 0;
            }
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

                fl = separate_freq_bands(outl, bars / 2, lcf, hcf, k, 1, sens, ignore);
                fr = separate_freq_bands(outr, bars / 2, lcf, hcf, k, 2, sens, ignore);
            } else {
                fftw_execute(pl);
                fl = separate_freq_bands(outl, bars, lcf, hcf, k, 1, sens, ignore);
            }
        } else { //**if in sleep mode wait and continue**//
            // wait 1 sec, then check sound again.
            req.tv_sec = 1;
            req.tv_nsec = 0;
            nanosleep(&req, NULL);
            continue;
        }

        // process [smoothing]

        if (monstercat) {
            if (stereo) {
                fl = monstercat_filter(fl, bars / 2, waves, monstercat);
                fr = monstercat_filter(fr, bars / 2, waves, monstercat);
            } else {
                fl = monstercat_filter(fl, bars, waves, monstercat);
            }
        }

        // preperaing signal for drawing
        for (o = 0; o < bars; o++) {
            if (stereo) {
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
        if (integral > 0) {
            for (o = 0; o < bars; o++) {
                f[o] = fmem[o] * integral + f[o];
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
        if (autosens) {
            for (o = 0; o < bars; o++) {
                if (f[o] > height) {
                    senseLow = false;
                    sens = sens * 0.985;
                    break;
                }
                if (senseLow && !silence)
                    sens = sens * 1.01;
                if (o == bars - 1)
                    sens = sens * 1.002;
            }
        }

        // output: draw processed input
        for (o = 0; o < bars; o++) {
            std::cout << std::min(f[o], height) << ' ';
        }
        std::cout << std::endl;

        req.tv_sec = 0;
        req.tv_nsec = 1000000000.0f / framerate;
        nanosleep(&req, NULL);
    }

    req.tv_sec = 0;
    req.tv_nsec = 100; // waiting some time to make sure audio is ready
    nanosleep(&req, NULL);

    //**telling audio thread to terminate**//
    audio.terminate = true;
    pthread_join(p_thread, NULL);

    return 0;
}
