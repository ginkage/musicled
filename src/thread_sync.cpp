#include "thread_sync.h"

#include <thread>

void ThreadSync::produce(std::function<void()> worker)
{
    std::lock_guard<std::mutex> locker(mux);
    worker();
    cv.notify_all();
}

void ThreadSync::consume(
    std::function<bool()> condition, std::function<void()> locked, std::function<void()> unlocked)
{
    // Our goal is to notify the consumer, but keep producer unlocked
    std::unique_lock<std::mutex> locker(mux);
    cv.wait(locker, condition);
    locked();
    locker.unlock();
    unlocked();
}
