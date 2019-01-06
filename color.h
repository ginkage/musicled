#ifndef __MUSICLED_COLOR_H__
#define __MUSICLED_COLOR_H__

union Color {
    unsigned long ic{ 0 };

    struct {
        unsigned char b;
        unsigned char g;
        unsigned char r;
    };
};

#endif
