#include "global_state.h"

#include <iostream>
#include <signal.h>
#include <string.h>

bool* g_terminate;

void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        std::cout << "CTRL-C pressed -- goodbye" << std::endl;
        *g_terminate = true;
    } else {
        signal(sig_no, SIG_DFL);
        raise(sig_no);
    }
}

GlobalState::GlobalState()
    : terminate(false)
{
    // Handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    g_terminate = &terminate;
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);
}
