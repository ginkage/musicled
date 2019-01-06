#ifndef __MUSICLED_FREQ_DATA_H__
#define __MUSICLED_FREQ_DATA_H__

#include "color.h"

struct FreqData {
    FreqData(int n1, unsigned int rate);
    ~FreqData();

    Color* const color;
    double* const note;
    int* const x;
    double* const left_amp;
    double* const right_amp;
    int minK, maxK;
};

#endif
