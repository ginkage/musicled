#pragma once

#include <vector>

class CircularBuffer {
public:
    CircularBuffer(int n);

    /* Replace oldest N values in the circular buffer with Values */
    void write(std::vector<double>& values);

    /* Retrieve N latest Values */
    void read(std::vector<double>& values);

private:
    std::vector<double> buffer;
    int size;
    int pos; // Position just after the last added value
};
