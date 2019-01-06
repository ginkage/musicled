#pragma once

#include <algorithm>
#include <chrono>
#include <thread>

using hires_clock = std::chrono::high_resolution_clock;

class VSync {
public:
    VSync(int frame_rate, hires_clock::time_point* prev = nullptr)
        : pstart(prev)
    {
        hires_clock::duration second = std::chrono::seconds(1);
        frame_time = second / frame_rate;
        start = (pstart != nullptr) ? (*pstart) : hires_clock::now();
    }

    ~VSync()
    {
        hires_clock::time_point finish = hires_clock::now();
        hires_clock::duration duration = finish - start;

        if (frame_time > duration)
            std::this_thread::sleep_for(frame_time - duration);

        if (pstart != nullptr)
            *pstart = std::max(start + frame_time, finish);
    }

private:
    hires_clock::time_point start;
    hires_clock::time_point* pstart;
    hires_clock::duration frame_time;
};
