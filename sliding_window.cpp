#include "sliding_window.h"

#include <algorithm>

SlidingWindow::SlidingWindow(int n)
    : buffer(n, 0)
    , size(n)
    , pos(0)
{
}

void SlidingWindow::write(std::vector<double>& values, int n)
{
    for (int k, j = 0; j < n; j += k) {
        k = std::min(pos + (n - j), size) - pos;
        std::copy_n(values.begin() + j, k, buffer.begin() + pos);
        pos = (pos + k) % size;
    }
}

void SlidingWindow::read(std::vector<double>& values, int n)
{
    int first = pos - n;
    while (first < 0)
        first += size;

    for (int k, j = 0; j < n; j += k) {
        k = std::min(first + (n - j), size) - first;
        std::copy_n(buffer.begin() + first, k, values.begin() + j);
        first = (first + k) % size;
    }
}
