#include "beat_detect.h"
#include "fps.h"

#include <iostream>
#include <memory>
#include <numeric>

BeatDetect::BeatDetect(GlobalState* state, std::shared_ptr<CircularBuffer<Sample>> buf,
    std::shared_ptr<ThreadSync> ts, std::shared_ptr<FreqData> freq, float sampleRate, int size)
    : global(state)
    , windowSize(size)
    , samples(buf)
    , values(windowSize)
    , sync(ts)
    , detector(sampleRate, windowSize, freq)
    , slide(std::chrono::seconds(5))
    , amps(windowSize * 2)
    , data(windowSize)
    , last_read(0)
    , last_written(0)
{
}

void BeatDetect::start_thread()
{
    thread = std::thread([this] { loop(); });
}

void BeatDetect::join_thread() { thread.join(); }

void BeatDetect::loop()
{
    Fps fps;
    while (!global->terminate) {
        sync->consume(
            [&] {
                // If we have enough data (or it's time to stop)
                return global->terminate || (samples->get_latest() >= windowSize);
            },
            [&] {
                // Then read the oldest samples while we're still locked to avoid missing data
                if (!global->terminate) {
                    // Chances are, we'll read everything up to the latest data
                    last_written = std::min(samples->get_latest() - last_read, windowSize);
                    last_read = samples->read_at(last_read, values.data(), last_written);
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

void BeatDetect::detect()
{
    // Write latest amp values to avoid calculating them twice
    for (unsigned int i = 0; i < last_written; ++i) {
        data[i] = std::abs(values[i]);
    }
    amps.write(data.data(), last_written);

    // Now read back the whole window
    amps.read(data.data(), windowSize);

    float bpm = detector.computeWindowBpm(data);
    bpm = slide.offer(std::make_pair(bpm, std::chrono::steady_clock::now()));
    if (!global->lock_bpm) {
        global->bpm = bpm;
    }
    // std::cout << "Window BPM: " << bpm << std::endl << std::flush;
}
