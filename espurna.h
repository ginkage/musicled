#pragma once

#include "global_state.h"

#include <string>
#include <thread>

class Espurna {
public:
    Espurna(std::string host, std::string api, GlobalState* state);
    void start_thread();
    void join_thread();

private:
    void socket_send();

    std::string hostname;
    std::string api_key;
    std::string resolved;
    GlobalState* global;
    std::thread thread;
};
