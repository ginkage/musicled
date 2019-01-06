#pragma once

#include <chrono>
#include <iostream>

using hires_clock = std::chrono::high_resolution_clock;

class Fps {
public:
    Fps()
        : frames(-1)
        , start(hires_clock::now())
    {
    }

    void tick(int framerate)
    {
        if (++frames >= framerate) {
            hires_clock::time_point finish = hires_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
            std::cout << (frames * 1.0e9 / duration) << " fps" << std::endl;
            frames = 0;
            start = finish;
        }
    }

private:
    int frames;
    hires_clock::time_point start;
};
