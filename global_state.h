#ifndef __MUSICLED_GLOBAL_H__
#define __MUSICLED_GLOBAL_H__

#include "color.h"

struct global_state {
    global_state();

    bool terminate{ false };
    color cur_color;
};

#endif
