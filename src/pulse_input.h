#pragma once

#include "audio_input.h"

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

// ALSA sound input implementation.
class PulseInput : public AudioInput {
public:
    PulseInput(GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~PulseInput();

    void on_context_state(pa_context* context);
    void on_server_info(pa_context* context, const pa_server_info* info);

private:
    void input_audio() override;

    pa_mainloop* mainloop;
    std::string source;
    pa_simple* connection;
};
