#ifndef __MUSICLED_VSYNC_H__
#define __MUSICLED_VSYNC_H__

#include <chrono>
#include <stdint.h>
#include <time.h>

class VSync {
public:
    VSync(int frame_rate, std::chrono::_V2::system_clock::time_point* prev = nullptr)
        : pstart(prev)
        , frame_time(1e9 / frame_rate)
    {
        start = (pstart != nullptr) ? (*pstart) : std::chrono::high_resolution_clock::now();
    }

    ~VSync()
    {
        std::chrono::_V2::system_clock::time_point finish = std::chrono::high_resolution_clock::now();
        int64_t duration = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

        if (duration < frame_time) {
            timespec req = {.tv_sec = 0, .tv_nsec = 0 };
            req.tv_sec = 0;
            req.tv_nsec = frame_time - duration;
            nanosleep(&req, NULL);

            if (pstart != nullptr)
                *pstart = start + std::chrono::nanoseconds(frame_time);
        } else if (pstart != nullptr)
            *pstart = finish;
    }

private:
    std::chrono::_V2::system_clock::time_point start;
    std::chrono::_V2::system_clock::time_point* pstart;
    int64_t frame_time;
};

#endif
