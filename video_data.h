#ifndef __MUSICLED_VIDEO_DATA_H__
#define __MUSICLED_VIDEO_DATA_H__

#include "spectrum.h"

#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>

struct video_data {
    video_data();
    ~video_data();
    void redraw(Spectrum& spec);

    Display* dis;
    int screen;
    Window win;
    GC gc;
    Atom close;
    Pixmap double_buffer = 0;
    unsigned int last_width = -1, last_height = -1;
};

#endif
