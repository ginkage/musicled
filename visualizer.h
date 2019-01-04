#ifndef __MUSICLED_VIDEO_DATA_H__
#define __MUSICLED_VIDEO_DATA_H__

#include "global_state.h"
#include "spectrum.h"

#include <X11/Xlib.h>

class Visualizer {
public:
    Visualizer(GlobalState* state);
    ~Visualizer();
    void redraw(Spectrum& spec);

private:
    bool handle_input();
    void handle_resize(Spectrum& spec);

    Display* dis;
    int screen;
    Window win;
    GC gc;
    Atom close;
    Pixmap double_buffer = 0;
    unsigned int last_width = -1, last_height = -1;
    GlobalState* global;
};

#endif
