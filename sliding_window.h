#ifndef __MUSICLED_SLIDING_WINDOW_H__
#define __MUSICLED_SLIDING_WINDOW_H__

class SlidingWindow {
public:
    SlidingWindow(int n);
    ~SlidingWindow();

    /* Replace oldest N values in the circular buffer with Values */
    void write(double* values, int n);

    /* Retrieve N latest Values */
    void read(double* values, int n);

private:
    double* buffer;
    int size;
    int pos; // Position just after the last added value
};

#endif
