#ifndef __MUSICLED_FREQ_DATA_H__
#define __MUSICLED_FREQ_DATA_H__

#include "color.h"

struct FreqData {
    FreqData(int n, unsigned int rate);
    ~FreqData();

    Color* color;
    unsigned long* ic;
    double* note;
    int* x;
    double* left_amp;
    double* right_amp;
    int minK, maxK;
};

#endif
