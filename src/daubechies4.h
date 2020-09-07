#include "wavelet.h"

class Daubechies4 : public Wavelet {
public:
    Daubechies4();

protected:
    decomposition forward(std::vector<double>& data) override;

private:
    double scalingDecom[4];
    double waveletDecom[4];
};
