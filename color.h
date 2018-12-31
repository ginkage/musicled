#ifndef __MUSICLED_COLOR_H__
#define __MUSICLED_COLOR_H__

struct color {
    int r{ 0 };
    int g{ 0 };
    int b{ 0 };

    bool operator!=(const color& that) const { return r != that.r || g != that.g || b != that.b; }
};

#endif
