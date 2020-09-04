#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

class ThreadSync {
public:
    void produce(std::function<void()> worker);
    void consume(std::function<bool()> condition, std::function<void()> locked,
        std::function<void()> unlocked);

private:
    std::mutex mux;
    std::condition_variable cv;
};
