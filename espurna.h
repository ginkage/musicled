#ifndef __MUSICLED_ESPURNA_H__
#define __MUSICLED_ESPURNA_H__

#include "color.h"

#include <pthread.h>

struct espurna {
public:
    espurna(char* host, char* api, color* col);
    void start_thread();
    void join_thread();

private:
    static void* run_thread(void* arg);
    void socket_send();

    char* hostname;
    char* api_key;
    char resolved[16];
    color* cur_color;
    pthread_t p_thread;
};

#endif
