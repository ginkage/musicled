#include "espurna.h"
#include "fps.h"
#include "global_state.h"
#include "spectrum.h"
#include "video_data.h"
#include "vsync.h"

#include <chrono>
#include <list>

int main(int argc, char* argv[])
{
    // Handle Ctrl+C
    global_state global;

    // Init Audio
    Spectrum spec(&global);

    // Init X11
    video_data video(&global);

    // Init Network
    std::list<espurna> strips;
    for (int k = 0; k + 2 < argc; k += 2)
        strips.push_back(espurna(argv[k + 1], argv[k + 2], &global));

    spec.start_input();
    for (espurna& strip : strips)
        strip.start_thread();

    const int framerate = 60;
    FPS fps;
    std::chrono::_V2::system_clock::time_point vstart = std::chrono::high_resolution_clock::now();
    while (!global.terminate) {
        VSync vsync(framerate, &vstart);
        fps.tick(framerate);
        spec.process();
        video.redraw(spec);
    }

    spec.stop_input();
    for (espurna& strip : strips)
        strip.join_thread();

    return 0;
}
