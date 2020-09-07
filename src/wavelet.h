#include <vector>

typedef std::pair<std::vector<double>, std::vector<double>> decomposition;

class Wavelet {
public:
    Wavelet() {}

    // 1-D forward transforms from time domain to all possible Hilbert domains
    std::vector<decomposition> decompose(std::vector<double>& data, int maxLevel);

protected:
    // 1-D forward transform from time domain to Hilbert domain
    virtual decomposition forward(std::vector<double>& data) = 0;
};
