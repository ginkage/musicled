#include "global_state.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

bool* g_terminate;

void sig_handler(int sig_no)
{
    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
        *g_terminate = true;
    } else {
        signal(sig_no, SIG_DFL);
        raise(sig_no);
    }
}

global_state::global_state()
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    g_terminate = &terminate;
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);
}
