#include "alsa_input.h"

#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <vector>

AlsaInput::AlsaInput(GlobalState* state)
    : global(state)
    , samples(65536)
{
    const char* audio_source = "hw:CARD=audioinjectorpi,DEV=0";

    // Open ALSA device to capture audio
    int err = snd_pcm_open(&handle, audio_source, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "error opening stream: " << snd_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params); // Allocate parameters on stack
    snd_pcm_hw_params_any(handle, params); // Set everything to defaults

    // Interleaved mode: right, left, right, left
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

    // Trying to set 16bit
    snd_pcm_format_t pcm_format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_hw_params_set_format(handle, params, pcm_format);

    // Assuming stereo
    unsigned int pcm_channels = 2;
    snd_pcm_hw_params_set_channels(handle, params, pcm_channels);

    // Trying our rate
    unsigned int pcm_rate = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &pcm_rate, NULL);

    // Number of frames per read
    snd_pcm_uframes_t pcm_frames = 256;
    snd_pcm_hw_params_set_period_size_near(handle, params, &pcm_frames, NULL);

    // Try setting the desired parameters
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        std::cerr << "Unable to set audio parameters: " << snd_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Prepare the audio interface
    err = snd_pcm_prepare(handle);
    if (err < 0) {
        std::cerr << "Cannot prepare audio interface for use: " << snd_strerror(err) << std::endl;
        exit(EXIT_FAILURE);
    }

    // Getting the actual format
    snd_pcm_hw_params_get_format(params, &pcm_format);

    // Converting the result to number of bits
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

    // Get the rest of the parameters
    rate = 0;
    frames = 0;
    channels = 0;
    snd_pcm_hw_params_get_rate(params, &rate, NULL);
    snd_pcm_hw_params_get_period_size(params, &frames, NULL);
    snd_pcm_hw_params_get_channels(params, &channels);

    // Hope it's successful
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
    // Note: in one-channel case, loff and roff will be equal
    // Bytes per sample, e.g. 2 for 16-bit, 3 for 24-bit, 4 for 32-bit
    const int bps = format / 8;
    // Bytes per frame, e.g. 4 for stereo (6 for 24-bit, 8 for 32-bit)
    const int stride = bps * channels;
    // Highest short in the first half of a frame, e.g. 0 (1 for 24-bit, 2 for 32-bit)
    const int loff = bps - 2;
    // Highest short in the second half of a frame, e.g. 2 (4 for 24-bit, 6 for 32-bit)
    const int roff = stride - 2;

    const double norm = 1.0 / 32768.0;

    std::vector<uint8_t> buffer(frames * stride);
    std::vector<Sample> data(frames);

    // Let's rock
    while (!global->terminate) {
        int n = snd_pcm_readi(handle, buffer.data(), frames);
        if (n == -EPIPE) {
            std::cerr << "Overrun occurred" << std::endl;
            // Try to recover
            snd_pcm_prepare(handle);
        } else if (n < 0) {
            std::cerr << "Error from read: " << snd_strerror(n) << std::endl;
        } else {
            // For 32 and 24 bit, we'll drop the extra precision
            int8_t* pleft = reinterpret_cast<int8_t*>(buffer.data() + loff);
            int8_t* pright = reinterpret_cast<int8_t*>(buffer.data() + roff);
            for (int i = 0; i < n; i++) {
                // Store normalized data within [-1, 1) range
                data[i] = Sample(*(int16_t*)pleft, *(int16_t*)pright) * norm;
                pleft += stride;
                pright += stride;
            }

            samples.write(data, n);
        }
    }
}
