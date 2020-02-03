#include "beat_energy.h"

BeatEnergy::BeatEnergy(
    GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts)
    : global(state)
    , samples(buf)
    , values(1024)
    , sync(ts)
{
}

void BeatEnergy::start_thread()
{
    thread = std::thread([this] { beat_detect(); });
}

void BeatEnergy::join_thread() { thread.join(); }

void BeatEnergy::beat_detect()
{
    int64_t last_read = 0;
    while (!global->terminate) {
        sync->consume(
            [&] {
                // If we have enough data (or it's time to stop)
                return global->terminate || (samples->get_latest() >= last_read + values.size());
            },
            [&] {
                // Then read the oldest samples while we're still locked to avoid missing data
                if (!global->terminate) {
                    last_read = samples->read_at(last_read, values.data(), values.size());
                }
            },
            [&] {
                // And continue the analysis after unlocking the mutex
                if (!global->terminate) {
                    // TODO: Analyze the samples
                }
            });
    }
}