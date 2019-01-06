#pragma once

#include "color.h"

struct GlobalState {
    GlobalState();

    bool terminate{ false };
    Color cur_Color;
};
