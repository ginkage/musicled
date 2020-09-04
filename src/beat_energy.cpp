#include "beat_energy.h"

#include <iostream>
#include <numeric>

BeatEnergy::BeatEnergy(
    GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts)
    : BeatDetect(state, buf, ts, 1024)
    , energy(43)
{
}

void BeatEnergy::detect()
{
    // Latest energy value
    double last_e = 0;
    for (Sample& sample : values) {
        last_e += std::norm(sample);
    }
    energy.write(&last_e, 1);

    // Check that we have enough data
    if (energy.get_latest() >= 43) {
        // All values, in arbitrary order
        std::vector<double>& v = energy.get_data();

        // Average energy
        double avg_e = std::accumulate(v.begin(), v.end(), 0.0) / v.size();

        // Energy variance
        double variance = 0;
        for (double e : v) {
            double diff = e - avg_e;
            variance += diff * diff;
        }
        variance /= v.size();

        // Do the linear regression
        double c = -0.0025714 * variance + 1.5142857;
        // std::cout << "E = " << last_e << ", Avg = " << avg_e << ", V = " << variance << ", C = "
        // << c << std::endl << std::flush;
        if (last_e > c * avg_e) {
            std::cout << "Boom! " << (1.5142857 - (last_e / avg_e)) / variance << std::endl
                      << std::flush;
        }
    }
}
