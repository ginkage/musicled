#include "audio_data.h"
#include "espurna.h"
#include "fps.h"
#include "global.h"
#include "spectrum.h"
#include "video_data.h"
#include "vsync.h"

#include <list>
#include <signal.h>
#include <stdio.h>
#include <string.h>

bool g_terminate{ false };

void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
        g_terminate = true;
    } else {
        signal(sig_no, SIG_DFL);
        raise(sig_no);
    }
}

int main(int argc, char* argv[])
{
    // Init Audio
    audio_data audio;

    // Init FFT
    Spectrum spec(&audio);

    // Handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);

    // Init Network
    std::list<espurna> strips;
    for (int k = 0; k + 2 < argc; k += 2)
        strips.push_back(espurna(argv[k + 1], argv[k + 2], &spec.curColor));

    // Init X11
    video_data video;

    audio.start_thread();
    for (espurna& strip : strips)
        strip.start_thread();

    const int framerate = 60;
    FPS fps;
    std::chrono::_V2::system_clock::time_point vstart = std::chrono::high_resolution_clock::now();
    while (!g_terminate) {
        VSync vsync(framerate, &vstart);
        fps.tick(framerate);
        spec.process();
        video.redraw(spec);
    }

    audio.join_thread();
    for (espurna& strip : strips)
        strip.join_thread();

    return 0;
}
