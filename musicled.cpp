#include "audio_data.h"
#include "color.h"
#include "espurna.h"
#include "fps.h"
#include "spectrum.h"
#include "video_data.h"
#include "vsync.h"

#include <list>
#include <signal.h>
#include <string.h>

audio_data* g_audio;

void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
        g_audio->terminate = true;
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
    g_audio = &audio;
    sigaction(SIGINT, &action, NULL);

    // Init Network
    std::list<espurna> strips;
    for (int k = 0; k + 2 < argc; k += 2) {
        char* hostname = argv[k + 1];
        char* api_key = argv[k + 2];
        char* addr = espurna::lookup(hostname);
        strips.push_back(espurna(hostname, api_key, addr, &audio));
    }

    // Init X11
    video_data video;

    audio.start_thread();
    for (espurna& strip : strips)
        strip.start_thread();

    const int framerate = 60;
    FPS fps;
    std::chrono::_V2::system_clock::time_point vstart = std::chrono::high_resolution_clock::now();
    while (!audio.terminate) {
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
