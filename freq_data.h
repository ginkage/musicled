#ifndef __MUSICLED_FREQ_DATA_H__
#define __MUSICLED_FREQ_DATA_H__

#include "color.h"

struct FreqData {
    Color c;
    unsigned long ic;
    double note;
    int x;
};

#endif
