#pragma once

#include <chrono>
#include <iostream>

using hires_clock = std::chrono::high_resolution_clock;

// Periodically compute and print the current frame rate
class Fps {
public:
    Fps()
        : frames(-1)
        , start(hires_clock::now())
    {
    }

    // One more frame begins
    void tick(int framerate)
    {
        if (++frames >= framerate) {
            hires_clock::time_point finish = hires_clock::now();
            auto duration
                = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
            std::cout << (frames * 1.0e9 / duration) << " fps" << std::endl;
            frames = 0;
            start = finish;
        }
    }

private:
    int frames; // Number of frames shown since the last print
    hires_clock::time_point start; // Time when last print occurred
};
