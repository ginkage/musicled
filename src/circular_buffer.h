#pragma once

#include <vector>

// Simple lock-free circular buffer implementation.
class CircularBuffer {
public:
    CircularBuffer(int n);

    // Replace oldest samples in the circular buffer with input values
    void write(std::vector<double>& values);

    // Retrieve latest samples in the circular buffer
    void read(double* values, int n);

private:
    std::vector<double> buffer; // Backing array
    int size; // Maximum number of frames to store
    int pos; // Position just after the last added value
};
