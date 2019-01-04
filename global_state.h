#ifndef __MUSICLED_GLOBAL_H__
#define __MUSICLED_GLOBAL_H__

#include "color.h"

struct GlobalState {
    GlobalState();

    bool terminate{ false };
    Color cur_Color;
};

#endif
