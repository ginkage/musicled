#include "wavelet.h"

class Daubechies8 : public Wavelet {
public:
    Daubechies8() {}

protected:
    // 1-D forward transform from time domain to Hilbert domain
    decomposition forward(std::vector<double>& data) override;

private:
    double scalingDecom[8] { -0.010597401784997278, 0.032883011666982945, 0.030841381835986965,
        -0.18703481171888114, -0.02798376941698385, 0.6308807679295904, 0.7148465705525415,
        0.23037781330885523 };

    double waveletDecom[8] { 0.23037781330885523, -0.7148465705525415, 0.6308807679295904,
        0.02798376941698385, -0.18703481171888114, -0.030841381835986965, 0.032883011666982945,
        0.010597401784997278 };
};
