#ifndef __MUSICLED_SLIDING_WINDOW_H__
#define __MUSICLED_SLIDING_WINDOW_H__

#include <algorithm>
#include <string.h>

template <class T> class SlidingWindow {
public:
    SlidingWindow(int n)
        : buffer(new T[n])
        , size(n)
        , pos(0)
    {
        memset(buffer, 0, n * sizeof(T));
    }

    ~SlidingWindow() { delete[] buffer; }

    /* Replace oldest N values in the circular buffer with Values */
    void write(T* values, int n)
    {
        for (int k, j = 0; j < n; j += k) {
            k = std::min(pos + (n - j), size) - pos;
            memcpy(buffer + pos, values + j, k * sizeof(T));
            pos = (pos + k) % size;
        }
    }

    /* Retrieve N latest Values */
    void read(T* values, int n)
    {
        int first = pos - n;
        while (first < 0)
            first += size;

        for (int k, j = 0; j < n; j += k) {
            k = std::min(first + (n - j), size) - first;
            memcpy(values + j, buffer + first, k * sizeof(T));
            first = (first + k) % size;
        }
    }

private:
    T* buffer;
    int size;
    int pos; // Position just after the last added value
};

#endif
