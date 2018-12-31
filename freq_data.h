#ifndef __MUSICLED_FREQ_DATA_H__
#define __MUSICLED_FREQ_DATA_H__

#include "color.h"

struct freq_data {
    color c;
    unsigned long ic;
    double note;
    int x;
};

#endif
