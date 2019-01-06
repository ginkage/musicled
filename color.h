#pragma once

union Color {
    unsigned long ic{ 0 };

    struct {
        unsigned char b;
        unsigned char g;
        unsigned char r;
    };
};
