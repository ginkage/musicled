#include "beat_energy.h"

#include <iostream>
#include <numeric>

BeatEnergy::BeatEnergy(
    GlobalState* state, CircularBuffer<Sample>* buf, std::shared_ptr<ThreadSync> ts)
    : global(state)
    , samples(buf)
    , energy(43)
    , values(1024)
    , sync(ts)
{
}

void BeatEnergy::start_thread()
{
    thread = std::thread([this] { beat_detect(); });
}

void BeatEnergy::join_thread() { thread.join(); }

void BeatEnergy::beat_detect()
{
    int64_t last_read = 0;
    while (!global->terminate) {
        sync->consume(
            [&] {
                // If we have enough data (or it's time to stop)
                return global->terminate || (samples->get_latest() >= last_read + values.size());
            },
            [&] {
                // Then read the oldest samples while we're still locked to avoid missing data
                if (!global->terminate) {
                    last_read = samples->read_at(last_read, values.data(), values.size());
                }
            },
            [&] {
                // And continue the analysis after unlocking the mutex
                if (!global->terminate) {
                    compute_energy();
                }
            });
    }
}

void BeatEnergy::compute_energy()
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
            std::cout << "Boom! " << (1.5142857 - (last_e / avg_e)) / variance << std::endl << std::flush;
        }
    }
}
