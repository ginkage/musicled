#pragma once

#include <algorithm>
#include <vector>

// Simple lock-free circular buffer implementation.
template <class T> class CircularBuffer {
public:
    CircularBuffer(int n)
        : buffer(n, 0)
        , size(n)
        , pos(0)
    {
    }

    // Replace oldest samples in the circular buffer with input values
    void write(std::vector<T>& values, int n)
    {
        // Write values to the buffer, *then* change the current position
        for (int k, j = 0; j < n; j += k) {
            k = std::min(pos + (n - j), size) - pos;
            std::copy_n(values.begin() + j, k, buffer.begin() + pos);
            pos = (pos + k) % size;
        }
    }

    // Retrieve latest samples in the circular buffer
    void read(T* values, int n)
    {
        // Read the current position, *then* read values
        int first = pos - n;
        while (first < 0)
            first += size;

        for (int k, j = 0; j < n; j += k) {
            k = std::min(first + (n - j), size) - first;
            std::copy_n(buffer.begin() + first, k, values + j);
            first = (first + k) % size;
        }
    }

private:
    std::vector<T> buffer; // Backing array
    int size; // Maximum number of frames to store
    int pos; // Position just after the last added value
};
