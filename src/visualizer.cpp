#include "visualizer.h"

#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <cmath>

Visualizer::Visualizer(GlobalState* state)
    : global(state)
    , back_buffer(0)
    , last_width(-1)
    , last_height(-1)
{
    // Initialize the X11 display
    dis = XOpenDisplay((char*)0);
    if (dis == nullptr)
        return;

    screen = DefaultScreen(dis);
    unsigned long black = BlackPixel(dis, screen), white = WhitePixel(dis, screen);

    // Create window
    win = XCreateSimpleWindow(dis, DefaultRootWindow(dis), 0, 0, 648, 360, 0, black, black);
    XSetStandardProperties(dis, win, "MusicLED", "Music", None, NULL, 0, NULL);
    XSelectInput(dis, win, ExposureMask | ButtonPressMask | KeyPressMask);

    gc = XCreateGC(dis, win, 0, 0);

    XClearWindow(dis, win);
    XMapRaised(dis, win);
    XSetForeground(dis, gc, white);

    // Listen for window close
    close = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dis, win, &close, 1);

    // Maximize window
    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.xclient.type = ClientMessage;
    xev.xclient.window = win;
    xev.xclient.message_type = XInternAtom(dis, "_NET_WM_STATE", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    xev.xclient.data.l[1] = XInternAtom(dis, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    xev.xclient.data.l[2] = XInternAtom(dis, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    xev.xclient.data.l[3] = 1;

    XSendEvent(dis, DefaultRootWindow(dis), False,
        SubstructureRedirectMask | SubstructureNotifyMask, &xev);
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

    // Check if the "q" key was pressed, or if the window was closed
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

    // The notes range is slightly wider than what we'll draw, to add a border
    double minNote = 34;
    double maxNote = 110;
    double kx = width / (maxNote - minNote);

    // If the window was resized...
    if (width != last_width || height != last_height) {
        last_width = width;
        last_height = height;

        // ...recreate the back buffer...
        if (back_buffer != 0)
            XFreePixmap(dis, back_buffer);
        back_buffer = XCreatePixmap(dis, win, width, height, 24);

        // ...and recalcualte the lines positions
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
    double ky = height / 64.0;
    double prevAmp = 0;
    int lastx = -1;
    int bottom = height;

    // Clear with black
    XSetForeground(dis, gc, 0);
    XFillRectangle(dis, back_buffer, gc, 0, 0, width, height);

    // Draw the lines
    for (int k = freq.minK; k < freq.maxK; k++) {
        prevAmp = std::max(prevAmp, freq.amp[k]);
        int x = freq.x[k];
        if (lastx < x) {
            lastx = x; // + 3; // Leave some space between the lines
            int y = bottom - prevAmp * ky - 0.5;
            prevAmp = 0;
            XSetForeground(dis, gc, freq.color[k].ic);
            XDrawLine(dis, back_buffer, gc, x, bottom, x, y);
        }
    }

    if (freq.ready) {
        int half = height / 2;
        int size = freq.wx.size();
        XSetForeground(dis, gc, 0xffffffff);
        lastx = width;
        int lasty = half, miny = half;
        for (int i = 0; i < size; i++) {
            int x = std::floor(freq.wx[i] * width + 0.5);
            int y = half - half * freq.wy[i];
            if (x == lastx) {
                miny = std::min(y, miny);
            } else {
                XDrawLine(dis, back_buffer, gc, lastx, lasty, x, miny);
                lastx = x;
                lasty = miny;
                miny = y;
            }
        }
    }

    // Show the result
    XCopyArea(dis, back_buffer, win, gc, 0, 0, width, height, 0, 0);
    XFlush(dis);
}
