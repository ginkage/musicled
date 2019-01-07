#include "visualizer.h"

#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <algorithm>

Visualizer::Visualizer(GlobalState* state)
    : global(state)
    , double_buffer(0)
    , last_width(-1)
    , last_height(-1)
{
    dis = XOpenDisplay((char*)0);
    if (dis == nullptr)
        return;

    screen = DefaultScreen(dis);
    unsigned long black = BlackPixel(dis, screen), white = WhitePixel(dis, screen);

    win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 648, 360, 0, black, black);
    XSetStandardProperties(dis, win, "MusicLED", "Music", None, NULL, 0, NULL);
    XSelectInput(dis, win, ExposureMask | ButtonPressMask | KeyPressMask);

    gc = XCreateGC(dis, win, 0, 0);

    XClearWindow(dis, win);
    XMapRaised(dis, win);
    XSetForeground(dis, gc, white);

    close = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dis, win, &close, 1);
};

Visualizer::~Visualizer()
{
    if (dis == nullptr)
        return;

    XFreeGC(dis, gc);
    XDestroyWindow(dis, win);
    XCloseDisplay(dis);
}

bool Visualizer::handle_input()
{
    if (dis == nullptr)
        return false;

    while (XPending(dis) > 0) {
        XEvent event;
        XNextEvent(dis, &event);
        if ((event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_q)
            || (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == close)) {
            global->terminate = true;
            return false;
        }
    }

    return true;
}

void Visualizer::handle_resize(FreqData& freq)
{
    unsigned int width, height, bw, dr;
    Window root;
    int xx, yy;
    XGetGeometry(dis, win, &root, &xx, &yy, &width, &height, &bw, &dr);

    double minNote = 34;
    double maxNote = 110;
    double kx = width / (maxNote - minNote);

    if (width != last_width || height != last_height) {
        last_width = width;
        last_height = height;
        if (double_buffer != 0)
            XFreePixmap(dis, double_buffer);
        double_buffer = XCreatePixmap(dis, win, width, height, 24);

        for (int k = freq.minK; k < freq.maxK; k++)
            freq.x[k] = (int)((freq.note[k] - minNote) * kx + 0.5);
    }
}

void Visualizer::redraw(FreqData& freq)
{
    if (!handle_input())
        return;
    handle_resize(freq);

    unsigned int width = last_width, height = last_height;
    double ky = height * 0.25 / 65536.0;
    double prevAmpL = 0;
    double prevAmpR = 0;
    int lastx = -1;

    XSetForeground(dis, gc, 0);
    XFillRectangle(dis, double_buffer, gc, 0, 0, width, height);

    for (int k = freq.minK; k < freq.maxK; k++) {
        prevAmpL = std::max(prevAmpL, freq.left_amp[k]);
        prevAmpR = std::max(prevAmpR, freq.right_amp[k]);
        int x = freq.x[k];
        if (lastx < x) {
            lastx = x + 3;
            int yl = height * 0.5 - prevAmpL * ky - 0.5;
            int yr = height * 0.5 + prevAmpR * ky + 0.5;
            prevAmpL = 0;
            prevAmpR = 0;
            XSetForeground(dis, gc, freq.color[k].ic);
            XDrawLine(dis, double_buffer, gc, x, yl, x, yr);
        }
    }

    XCopyArea(dis, double_buffer, win, gc, 0, 0, width, height, 0, 0);
    XFlush(dis);
}
