#include "alsa_input.h"

#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

AlsaInput::AlsaInput(GlobalState* state)
    : global(state)
    , left(65536)
    , right(65536)
{
    const char* audio_source = "hw:CARD=audioinjectorpi,DEV=0";

    // alsa: open device to capture audio
    int err = snd_pcm_open(&handle, audio_source, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "error opening stream: " << snd_strerror(err) << std::endl;
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
        std::cerr << "unable to set hw parameters: " << snd_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    err = snd_pcm_prepare(handle);
    if (err < 0) {
        std::cerr << "cannot prepare audio interface for use: " << snd_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    // getting actual format
    snd_pcm_hw_params_get_format(params, &pcm_format);

    // converting result to number of bits
    switch (pcm_format) {
    case SND_PCM_FORMAT_S16_LE:
        format = 16;
        break;
    case SND_PCM_FORMAT_S24_LE:
        format = 24;
        break;
    case SND_PCM_FORMAT_S32_LE:
        format = 32;
        break;
    default:
        format = -1;
        break;
    }

    rate = 0;
    frames = 0;
    channels = 0;
    snd_pcm_hw_params_get_rate(params, &rate, NULL);
    snd_pcm_hw_params_get_period_size(params, &frames, NULL);
    snd_pcm_hw_params_get_channels(params, &channels);

    if (format == -1 || rate == 0) {
        std::cerr << "Could not get rate and/or format, quiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
}

AlsaInput::~AlsaInput() { snd_pcm_close(handle); }

void AlsaInput::start_thread()
{
    thread = std::thread([=] { input_alsa(); });
}

void AlsaInput::join_thread() { thread.join(); }

void AlsaInput::input_alsa()
{
    const int bps = format / 8; // bytes per sample
    const int stride = bps * channels; // bytes per frame
    const int loff = bps - 2; // Highest 2 bytes in the first half of a frame
    const int roff = stride - 2; // Highest 2 bytes in the second half of a frame
    const int size = frames * stride;

    std::vector<uint8_t> buffer(size);
    std::vector<double> left_data(frames);
    std::vector<double> right_data(frames);

    while (!global->terminate) {
        int err = snd_pcm_readi(handle, buffer.data(), frames);
        if (err == -EPIPE) {
            std::cerr << "overrun occurred" << std::endl;
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            std::cerr << "error from read: " << snd_strerror(err) << std::endl;
        } else if (err != (int)frames) {
            std::cerr << "short read, read " << err << " of " << frames << " frames" << std::endl;
        } else {
            // For 32 and 24 bit, we'll drop extra precision
            int8_t* pleft = (int8_t*)(buffer.data() + loff);
            int8_t* pright = (int8_t*)(buffer.data() + roff);
            for (unsigned int i = 0; i < frames; i++) {
                left_data[i] = *(int16_t*)pleft;
                right_data[i] = *(int16_t*)pright;
                pleft += stride;
                pright += stride;
            }

            left.write(left_data);
            right.write(right_data);
        }
    }
}
