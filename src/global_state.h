#pragma once

#include "color.h"

// Contains cross-thread current LED strip color and termination state
struct GlobalState {
    GlobalState();

    bool terminate;
    Color cur_color;
    Color send_color;
    float bpm;
    bool lock_bpm;
};
