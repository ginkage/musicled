#pragma once

#include "color.h"
#include "global_state.h"
#include "thread_sync.h"

#include <thread>

// Helper "consumer" class to sync various LED strips
class LedStrip {
public:
    LedStrip(GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~LedStrip();

    void start_thread();
    virtual void startup() = 0;
    virtual void shutdown() = 0;
    virtual void set_rgb(Color c) = 0;

private:
    void socket_send();

    GlobalState* global; // Global state for thread termination
    std::thread thread; // Output thread
    std::shared_ptr<ThreadSync> sync;
};
