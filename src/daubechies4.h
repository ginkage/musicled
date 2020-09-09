#pragma once

#include "wavelet.h"

class Daubechies4 : public Wavelet {
public:
    Daubechies4();

protected:
    void forward(std::vector<double>& data, decomposition& out) override;

private:
    double scalingDecom[4];
    double waveletDecom[4];
};
