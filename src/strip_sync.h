#pragma once

#include "color.h"
#include "global_state.h"
#include "thread_sync.h"

#include <thread>

// Helper "producer" class to sync various LED strips
class StripSync {
public:
    StripSync(GlobalState* state);
    ~StripSync();

    std::shared_ptr<ThreadSync> get() { return sync; }

protected:
    void start_thread();
    void join_thread();
    void loop();

    GlobalState* global; // Global state for thread termination
    std::thread thread; // Compute thread
    std::shared_ptr<ThreadSync> sync;
};
