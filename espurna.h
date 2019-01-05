#ifndef __MUSICLED_ESPURNA_H__
#define __MUSICLED_ESPURNA_H__

#include "global_state.h"

#include <thread>

class Espurna {
public:
    Espurna(char* host, char* api, GlobalState* state);
    void start_thread();
    void join_thread();

private:
    void socket_send();

    char* hostname;
    char* api_key;
    char resolved[16];
    GlobalState* global;
    std::thread thread;
};

#endif
