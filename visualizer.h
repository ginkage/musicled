#pragma once

#include "freq_data.h"
#include "global_state.h"

#include <X11/Xlib.h>

// Simple X11 visualizer that shows colorful stereo amplitudes.
// Closing the visualizer window causes the global state to stop all threads.
class Visualizer {
public:
    Visualizer(GlobalState* state);
    ~Visualizer();

    // Draw the visualization if a display is present
    void redraw(FreqData& spec);

private:
    bool handle_input();
    void handle_resize(FreqData& freq);

    GlobalState* global;
    Display* dis; // X11 display
    int screen; // X11 screen
    Window win; // X11 window
    GC gc; // X11 graphic context
    Atom close; // Window deletion event
    Pixmap double_buffer; // Frame it's all actually rendered into
    unsigned int last_width, last_height; // Cached window size
};
