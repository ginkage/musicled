#pragma once

#include "beat_detect.h"
#include "circular_buffer.h"
#include "freq_data.h"
#include "global_state.h"
#include "sample.h"
#include "sliding_median.h"
#include "thread_sync.h"
#include "wavelet_bpm_detector.h"

#include <chrono>
#include <memory>
#include <thread>

class BeatDetect {
public:
    BeatDetect(GlobalState* state, std::shared_ptr<CircularBuffer<Sample>> buf,
        std::shared_ptr<ThreadSync> ts, std::shared_ptr<FreqData> freq, float sampleRate,
        int windowSize);

    void start_thread();
    void join_thread();

protected:
    void loop();
    void detect();

    GlobalState* global; // Global state for thread termination
    int64_t windowSize;
    std::shared_ptr<CircularBuffer<Sample>> samples; // Audio samples
    std::vector<Sample> values;
    std::thread thread; // Compute thread
    std::shared_ptr<ThreadSync> sync;

    WaveletBPMDetector detector;

    using Timestamp = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::steady_clock::duration;
    SlidingMedian<float, Timestamp, Duration> slide;
    CircularBuffer<float> amps;
    std::vector<float> data;
    int64_t last_read;
    unsigned int last_written;
};
