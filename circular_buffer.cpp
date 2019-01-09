#include "circular_buffer.h"

#include <algorithm>

CircularBuffer::CircularBuffer(int n)
    : buffer(n, 0)
    , size(n)
    , pos(0)
{
}

void CircularBuffer::write(std::vector<double>& values)
{
    // Write values to the buffer, *then* change the current position
    for (int k, j = 0, n = values.size(); j < n; j += k) {
        k = std::min(pos + (n - j), size) - pos;
        std::copy_n(values.begin() + j, k, buffer.begin() + pos);
        pos = (pos + k) % size;
    }
}

void CircularBuffer::read(double* values, int n)
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
