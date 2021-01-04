#pragma once

#include "global_state.h"
#include "led_strip.h"
#include "thread_sync.h"

#include <curl/curl.h>
#include <string>

// Control ESPurna LED strip using RESTless HTTP API.
// Object creation spawns a thread that periodically sends the current color
// from the global state to the specified host name, destruction joins it.
class Espurna : public LedStrip {
public:
    Espurna(std::string host, std::string api, GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~Espurna();

    void startup() override;
    void shutdown() override;
    void set_rgb(Color c) override;

private:
    void send_message(const char* topic, const char* value);

    CURL* curl;
    std::string hostname; // Host name of the LED strip
    std::string api_key; // API key for the RESTless API
};
