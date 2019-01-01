#include "video_data.h"
#include "global.h"

video_data::video_data()
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

video_data::~video_data()
{
    if (dis == nullptr)
        return;

    XFreeGC(dis, gc);
    XDestroyWindow(dis, win);
    XCloseDisplay(dis);
}

void video_data::redraw(Spectrum& spec)
{
    if (dis == nullptr)
        return;

    while (XPending(dis) > 0) {
        XEvent event;
        XNextEvent(dis, &event);
        if ((event.type == KeyPress && XLookupKeysym(&event.xkey, 0) == XK_q)
            || (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == close)) {
            g_terminate = true;
            return;
        }
    }

    unsigned int width, height, bw, dr;
    Window root;
    int xx, yy;
    XGetGeometry(dis, win, &root, &xx, &yy, &width, &height, &bw, &dr);

    freq_data* freq = spec.freq;
    double minNote = 34;
    double maxNote = 110;
    double kx = width / (maxNote - minNote);
    double ky = height * 0.25 / 65536.0;
    int minK = spec.minK, maxK = spec.maxK;

    if (width != last_width || height != last_height) {
        last_width = width;
        last_height = height;
        if (double_buffer != 0)
            XFreePixmap(dis, double_buffer);
        double_buffer = XCreatePixmap(dis, win, width, height, 24);

        for (int k = minK; k < maxK; k++)
            freq[k].x = (int)((freq[k].note - minNote) * kx + 0.5);
    }

    XSetForeground(dis, gc, 0);
    XFillRectangle(dis, double_buffer, gc, 0, 0, width, height);

    int lastx = -1;
    double prevAmpL = 0;
    double prevAmpR = 0;

    for (int k = minK; k < maxK; k++) {
        prevAmpL = std::max(prevAmpL, spec.left.amp[k]);
        prevAmpR = std::max(prevAmpR, spec.right.amp[k]);
        int x = freq[k].x;
        if (lastx < x) {
            lastx = x + 3;
            int yl = (int)(prevAmpL * ky + 0.5);
            int yr = (int)(prevAmpR * ky + 0.5);
            prevAmpL = 0;
            prevAmpR = 0;
            XSetForeground(dis, gc, freq[k].ic);
            XDrawLine(dis, double_buffer, gc, x, height * 0.5 - yl, x, height * 0.5 + yr);
        }
    }

    XCopyArea(dis, double_buffer, win, gc, 0, 0, width, height, 0, 0);
    XFlush(dis);
}
