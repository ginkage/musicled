#include "pulse_input.h"

#include <cinttypes>
#include <iostream>
#include <stdlib.h>
#include <vector>

void server_info_cb(pa_context* context, const pa_server_info* info, void* userdata)
{
    PulseInput* input = reinterpret_cast<PulseInput*>(userdata);
    input->on_server_info(context, info);
}

void context_state_cb(pa_context* context, void* userdata)
{
    PulseInput* input = reinterpret_cast<PulseInput*>(userdata);
    input->on_context_state(context);
}

PulseInput::PulseInput(GlobalState* state, std::shared_ptr<ThreadSync> ts)
    : AudioInput(state, ts)
{
    // Create a mainloop API and connection to the default server.
    mainloop = pa_mainloop_new();
    pa_mainloop_api* mainloop_api = pa_mainloop_get_api(mainloop);
    pa_context* context = pa_context_new(mainloop_api, "musicled device list");
    pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
    pa_context_set_state_callback(context, context_state_cb, this);

    // Start the mainloop to get default sink.
    // Starting with one nonblokng iteration in case pulseaudio is not able to run.
    int ret;
    if (!(ret = pa_mainloop_iterate(mainloop, 0, &ret))) {
        std::cerr << "Could not open pulseaudio mainloop to find default device name: " << ret
                  << std::endl;
        std::cerr << "check if pulseaudio is running" << std::endl;
        exit(EXIT_FAILURE);
    }

    // After this call, "source" should be initialized.
    pa_mainloop_run(mainloop, &ret);
    std::cout << source << std::endl;

    int error;
    unsigned int frag_size = frames * channels * format / 8;
    pa_buffer_attr attr = {
        .maxlength = (uint32_t)-1, .tlength = 0, .prebuf = 0, .minreq = 0, .fragsize = frag_size
    };
    pa_sample_spec spec
        = { .format = PA_SAMPLE_S16LE, .rate = rate, .channels = (uint8_t)channels };
    if (!(connection = pa_simple_new(nullptr, "musicled", PA_STREAM_RECORD, source.c_str(),
              "audio for musicled", &spec, nullptr, &attr, &error))) {
        std::cerr << "Could not open pulseaudio source: " << source << ", " << pa_strerror(error)
                  << std::endl;
        std::cerr << "To find a list of your pulseaudio sources run 'pacmd list-sources'"
                  << std::endl;
        exit(EXIT_FAILURE);
    }
}

PulseInput::~PulseInput() { pa_simple_free(connection); }

void PulseInput::on_context_state(pa_context* context)
{
    // make sure loop is ready
    switch (pa_context_get_state(context)) {
    case PA_CONTEXT_UNCONNECTED:
        // printf("UNCONNECTED\n");
        break;
    case PA_CONTEXT_CONNECTING:
        // printf("CONNECTING\n");
        break;
    case PA_CONTEXT_AUTHORIZING:
        // printf("AUTHORIZING\n");
        break;
    case PA_CONTEXT_SETTING_NAME:
        // printf("SETTING_NAME\n");
        break;
    case PA_CONTEXT_READY: // extract default sink name
        // printf("READY\n");
        pa_operation_unref(pa_context_get_server_info(context, server_info_cb, this));
        break;
    case PA_CONTEXT_FAILED:
        std::cerr << "failed to connect to pulseaudio server" << std::endl;
        exit(EXIT_FAILURE);
        break;
    case PA_CONTEXT_TERMINATED:
        // printf("TERMINATED\n");
        pa_mainloop_quit(mainloop, 0);
        break;
    }
}

void PulseInput::on_server_info(pa_context* context, const pa_server_info* info)
{
    source = info->default_source_name;
    // source = info->default_sink_name;
    // source += ".monitor";

    pa_context_disconnect(context);
    pa_context_unref(context);
    pa_mainloop_quit(mainloop, 0);
    pa_mainloop_free(mainloop);
}

void PulseInput::input_audio()
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

    // Absolute value of all samples will stay wihin [0, 1]
    const float norm = 1.0f / (32768.0f * std::sqrt(2.0f));

    std::vector<uint8_t> buffer(frames * stride);
    std::vector<Sample> data(frames);

    // Let's rock
    while (!global->terminate) {
        int error;
        if (pa_simple_read(connection, buffer.data(), buffer.size(), &error) < 0) {
            std::cerr << "Error from read: " << pa_strerror(error) << std::endl;
        } else {
            // For 32 and 24 bit, we'll drop the extra precision
            int8_t* pleft = reinterpret_cast<int8_t*>(buffer.data() + loff);
            int8_t* pright = reinterpret_cast<int8_t*>(buffer.data() + roff);
            for (unsigned int i = 0; i < frames; i++) {
                // Store normalized data within [-1, 1) range
                data[i] = Sample(*(int16_t*)pleft, *(int16_t*)pright) * norm;
                pleft += stride;
                pright += stride;
            }

            sync->produce([&] { samples->write(data.data(), frames); });
        }
    }

    sync->produce([] {}); // No-op to release the consumer thread
}
