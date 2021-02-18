#pragma once

#include <algorithm>
#include <chrono>
#include <thread>

using hires_clock = std::chrono::high_resolution_clock;

// Helper class to throttle loops to the specified rate in a simplest way.
class VSync {
public:
    VSync(float frame_rate, hires_clock::time_point* prev = nullptr)
        : pstart(prev)
    {
        hires_clock::duration second = std::chrono::seconds(1);
        frame_time = std::chrono::duration_cast<hires_clock::duration>(second / frame_rate);

        // If there is some info about the previous frame, use it!
        start = (pstart != nullptr) ? (*pstart) : hires_clock::now();
    }

    ~VSync()
    {
        hires_clock::time_point finish = hires_clock::now();
        hires_clock::duration duration = finish - start;

        // All done with some time to spare, time for a nap
        if (frame_time > duration)
            std::this_thread::sleep_for(frame_time - duration);

        // If we took longer, the next frame should be rendered immediately
        if (pstart != nullptr)
            *pstart = std::max(start + frame_time, finish);
    }

private:
    hires_clock::time_point start; // When the current frame started
    hires_clock::time_point* pstart; // When it should have started
    hires_clock::duration frame_time; // Target frame duration
};
