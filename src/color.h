#pragma once

// Union for storing and setting X11 integer color per component.
union Color {
    unsigned long ic{ 0 }; // X11 color

    struct {
        unsigned char b;
        unsigned char g;
        unsigned char r;
        unsigned char a;
    };
};
