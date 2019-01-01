#ifndef __MUSICLED_ESPURNA_H__
#define __MUSICLED_ESPURNA_H__

#include "global_state.h"

#include <pthread.h>

struct espurna {
public:
    espurna(char* host, char* api, global_state* state);
    void start_thread();
    void join_thread();

private:
    static void* run_thread(void* arg);
    void socket_send();

    char* hostname;
    char* api_key;
    char resolved[16];
    global_state* global;
    pthread_t p_thread;
};

#endif
