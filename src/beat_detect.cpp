#include "beat_detect.h"
#include "fps.h"

#include <iostream>
#include <memory>
#include <numeric>

BeatDetect::BeatDetect(GlobalState* state, std::shared_ptr<CircularBuffer<Sample>> buf,
    std::shared_ptr<ThreadSync> ts, int size)
    : global(state)
    , samples(buf)
    , values(size)
    , sync(ts)
{
}

void BeatDetect::start_thread()
{
    thread = std::thread([this] { loop(); });
}

void BeatDetect::join_thread() { thread.join(); }

void BeatDetect::loop()
{
    // int64_t last_read = 0;
    Fps fps;
    while (!global->terminate) {
        sync->consume(
            [&] {
                // If we have enough data (or it's time to stop)
                return global->terminate
                    // || (samples->get_latest() >= last_read + (int64_t)values.size());
                    || (samples->get_latest() >= values.size());
            },
            [&] {
                // Then read the oldest samples while we're still locked to avoid missing data
                if (!global->terminate) {
                    // last_read = samples->read_at(last_read, values.data(), values.size());
                    samples->read(values.data(), values.size());
                }
            },
            [&] {
                // And continue the analysis after unlocking the mutex
                if (!global->terminate) {
                    detect();
                }
            });
        fps.tick(50);
    }
}
