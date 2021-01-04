#include "espurna.h"

#include <iostream>
#include <stdio.h>

size_t write_data(void* buffer __attribute__((unused)), size_t size, size_t nmemb,
    void* userp __attribute__((unused)))
{
    return size * nmemb;
}

Espurna::Espurna(
    std::string host, std::string api, GlobalState* state, std::shared_ptr<ThreadSync> ts)
    : LedStrip(state, ts)
    , hostname(host)
    , api_key(api)
{
    curl_global_init(CURL_GLOBAL_NOTHING);

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to init libCURL" << std::endl;
        exit(EXIT_FAILURE);
    }

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    // Start the output thread immediately
    start_thread();
}

Espurna::~Espurna() { curl_easy_cleanup(curl); }

void Espurna::send_message(const char* topic, const char* value)
{
    char request[128], content[128];
    snprintf(request, sizeof(request), "http://%s/api/%s", hostname.data(), topic);
    snprintf(content, sizeof(content), "apikey=%s&value=%s", api_key.c_str(), value);

    curl_easy_setopt(curl, CURLOPT_URL, request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content);
    curl_easy_perform(curl);
}

void Espurna::startup() { send_message("relay/0", "1"); }

void Espurna::shutdown() { send_message("relay/0", "0"); }

void Espurna::set_rgb(Color c)
{
    char value[16];
    snprintf(value, sizeof(value), "%d,%d,%d", c.r, c.g, c.b);
    send_message("rgb", value);
}
