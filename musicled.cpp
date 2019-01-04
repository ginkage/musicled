#include "espurna.h"
#include "fps.h"
#include "freq_data.h"
#include "global_state.h"
#include "spectrum.h"
#include "visualizer.h"
#include "vsync.h"

#include <chrono>
#include <list>

int main(int argc, char* argv[])
{
    // Handle Ctrl+C
    GlobalState global;

    // Init Audio
    Spectrum spec(&global);

    // Init X11
    Visualizer video(&global);

    // Init Network
    std::list<Espurna> strips;
    for (int k = 0; k + 2 < argc; k += 2)
        strips.push_back(Espurna(argv[k + 1], argv[k + 2], &global));

    spec.start_input();
    for (Espurna& strip : strips)
        strip.start_thread();

    const int framerate = 60;
    Fps fps;
    std::chrono::_V2::system_clock::time_point vstart = std::chrono::high_resolution_clock::now();
    while (!global.terminate) {
        VSync vsync(framerate, &vstart);
        fps.tick(framerate);
        FreqData& freq = spec.process();
        video.redraw(freq);
    }

    spec.stop_input();
    for (Espurna& strip : strips)
        strip.join_thread();

    return 0;
}
