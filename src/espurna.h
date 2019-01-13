#pragma once

#include "global_state.h"

#include <curl/curl.h>
#include <string>
#include <thread>

// Control ESPurna LED strip using RESTless HTTP API.
// Object creation spawns a thread that periodically sends the current color
// from the global state to the specified host name, destruction joins it.
class Espurna {
public:
    Espurna(std::string host, std::string api, GlobalState* state);
    ~Espurna();

private:
    void send_message(const char* topic, const char* value, const char* api);
    void socket_send();

    CURL* curl;
    std::string hostname; // Host name of the LED strip
    std::string api_key; // API key for the RESTless API
    GlobalState* global; // Global state for thread termination
    std::thread thread; // Output thread
};
