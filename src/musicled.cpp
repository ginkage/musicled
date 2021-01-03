#include "espurna.h"
#include "flux_led.h"
#include "fps.h"
#include "freq_data.h"
#include "global_state.h"
#include "spectrum.h"
#include "strip_sync.h"
#include "visualizer.h"
#include "vsync.h"

#include <chrono>
#include <list>

using hires_clock = std::chrono::high_resolution_clock;

int main(int argc, char* argv[])
{
    // Init global state for input and output threads
    GlobalState global;

    // Init Audio (and start audio input thread)
    Spectrum spec(&global, 2048);

    // Init X11
    Visualizer video(&global);

    StripSync sync(&global);

    // Init Network (and spawn an output thread for every LED strip)
    std::list<Espurna> strips;
    for (int k = 0; k + 2 < argc; k += 2)
        strips.emplace_back(argv[k + 1], argv[k + 2], &global, sync.get());

    std::list<FluxLed> magic;
    if (argc % 2 == 0)
        magic.emplace_back(argv[argc - 1], &global, sync.get());

    const int framerate = 60;
    // Fps fps;
    hires_clock::time_point vstart = hires_clock::now();
    while (!global.terminate) {
        VSync vsync(framerate, &vstart);
        // fps.tick(framerate);
        FreqData& freq = spec.process();
        video.redraw(freq);
    }

    return 0;
}
