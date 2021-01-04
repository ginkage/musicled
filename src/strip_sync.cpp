#include "strip_sync.h"
#include "fps.h"
#include "vsync.h"

StripSync::StripSync(GlobalState* state)
    : global(state)
    , sync(new ThreadSync())
{
    start_thread();
}

StripSync::~StripSync() { join_thread(); }

void StripSync::start_thread()
{
    thread = std::thread([this] { loop(); });
}

void StripSync::join_thread() { thread.join(); }

void StripSync::loop()
{
    // Fps fps;
    hires_clock::time_point vcheck = hires_clock::now(), vsend = vcheck;
    while (!global->terminate) {
        VSync vsync(60, &vcheck); // Wait between checks
        if (global->send_color.ic != global->cur_color.ic) {
            global->send_color.ic = global->cur_color.ic;

            // The color has changed, send it to the LED strips!
            VSync vsync(2 * global->bpm / 60.0, &vsend); // Wait between sending
            // fps.tick(8);
            sync->produce([] {});
        }
    }

    sync->produce([] {}); // No-op to release the consumer thread
}
