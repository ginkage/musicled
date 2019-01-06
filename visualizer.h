#pragma once

#include "freq_data.h"
#include "global_state.h"

#include <X11/Xlib.h>

class Visualizer {
public:
    Visualizer(GlobalState* state);
    ~Visualizer();
    void redraw(FreqData& spec);

private:
    bool handle_input();
    void handle_resize(FreqData& freq);

    Display* dis;
    int screen;
    Window win;
    GC gc;
    Atom close;
    Pixmap double_buffer = 0;
    unsigned int last_width = -1, last_height = -1;
    GlobalState* global;
};
