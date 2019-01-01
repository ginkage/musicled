#ifndef __MUSICLED_VIDEO_DATA_H__
#define __MUSICLED_VIDEO_DATA_H__

#include "global_state.h"
#include "spectrum.h"

#include <X11/Xlib.h>

struct video_data {
public:
    video_data(global_state* state);
    ~video_data();
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
    global_state* global;
};

#endif
