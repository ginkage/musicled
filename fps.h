#ifndef __MUSICLED_Fps_H__
#define __MUSICLED_Fps_H__

#include <chrono>
#include <iostream>
#include <stdint.h>

class Fps {
private:
    int frames;
    std::chrono::_V2::system_clock::time_point start;

public:
    Fps()
        : frames(-1)
        , start(std::chrono::high_resolution_clock::now())
    {
    }

    void tick(int framerate)
    {
        if (++frames >= framerate) {
            std::chrono::_V2::system_clock::time_point finish = std::chrono::high_resolution_clock::now();
            int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
            std::cout << (frames * 1.0e9 / duration) << " fps" << std::endl;
            frames = 0;
            start = finish;
        }
    }
};

#endif
