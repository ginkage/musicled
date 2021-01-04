#include "led_strip.h"

LedStrip::LedStrip(GlobalState* state, std::shared_ptr<ThreadSync> ts)
    : global(state)
    , sync(ts)
{
}

LedStrip::~LedStrip() { thread.join(); }

void LedStrip::start_thread()
{
    thread = std::thread([this] { socket_send(); });
}

void LedStrip::socket_send()
{
    startup();

    Color col; // Last sent color
    while (!global->terminate) {
        sync->consume(
            [&] {
                // If the color has changed (or it's time to stop)
                return global->terminate || (col.ic != global->send_color.ic);
            },
            [&] {
                // Then cache the color locally
                if (!global->terminate) {
                    col.ic = global->send_color.ic;
                }
            },
            [&] {
                // And continue sending the color after unlocking the mutex
                if (!global->terminate) {
                    set_rgb(col);
                }
            });
    }

    shutdown();
}
