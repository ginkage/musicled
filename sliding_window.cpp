#include "sliding_window.h"

#include <algorithm>
#include <string.h>

SlidingWindow::SlidingWindow(int n)
    : buffer(new double[n])
    , size(n)
    , pos(0)
{
    memset(buffer, 0, n * sizeof(double));
}

SlidingWindow::~SlidingWindow() { delete[] buffer; }

void SlidingWindow::write(double* values, int n)
{
    for (int k, j = 0; j < n; j += k) {
        k = std::min(pos + (n - j), size) - pos;
        memcpy(buffer + pos, values + j, k * sizeof(double));
        pos = (pos + k) % size;
    }
}

void SlidingWindow::read(double* values, int n)
{
    int first = pos - n;
    while (first < 0)
        first += size;

    for (int k, j = 0; j < n; j += k) {
        k = std::min(first + (n - j), size) - first;
        memcpy(values + j, buffer + first, k * sizeof(double));
        first = (first + k) % size;
    }
}
