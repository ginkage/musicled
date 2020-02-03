#pragma once

#include <algorithm>
#include <cinttypes>
#include <vector>

// Simple lock-free circular buffer implementation.
template <class T> class CircularBuffer {
public:
    CircularBuffer(int n)
        : buffer(n, 0)
        , size(n)
        , pos(0)
        , overwritten(0)
        , total_written(0)
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

        total_written += n;

        if (total_written > size) {
            overwritten = total_written - size;
        }
    }

    // Retrieve latest samples in the circular buffer
    void read(T* values, int n) { read_at(total_written - n, values, n); }

    // Retrieve samples at the specified position
    int64_t read_at(int64_t from, T* values, int n)
    {
        from = std::max(from, overwritten);

        // Read the current position, *then* read values
        int first = pos - (total_written - from);
        while (first < 0)
            first += size;

        for (int k, j = 0; j < n; j += k) {
            k = std::min(first + (n - j), size) - first;
            std::copy_n(buffer.begin() + first, k, values + j);
            first = (first + k) % size;
        }

        return from + n;
    }

    int64_t get_latest() { return total_written; }

private:
    std::vector<T> buffer; // Backing array
    int size; // Maximum number of frames to store
    int pos; // Position just after the last added value

    int64_t overwritten;
    int64_t total_written;
};
