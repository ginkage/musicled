#include "espurna.h"
#include "vsync.h"

#include <iostream>
#include <stdio.h>

size_t write_data(void* buffer __attribute__((unused)), size_t size, size_t nmemb,
    void* userp __attribute__((unused)))
{
    return size * nmemb;
}

Espurna::Espurna(std::string host, std::string api, GlobalState* state)
    : hostname(host)
    , api_key(api)
    , global(state)
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
    thread = std::thread([this] { socket_send(); });
}

Espurna::~Espurna()
{
    thread.join();
    curl_easy_cleanup(curl);
}

void Espurna::send_message(const char* topic, const char* value, const char* api)
{
    char request[128], content[128];
    snprintf(request, sizeof(request), "http://%s/api/%s", hostname.data(), topic);
    snprintf(content, sizeof(content), "apikey=%s&value=%s", api, value);

    curl_easy_setopt(curl, CURLOPT_URL, request);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, content);
    curl_easy_perform(curl);
}

void Espurna::socket_send()
{
    const char* api = api_key.c_str();
    send_message("relay/0", "1", api);

    Color col; // Last sent color
    char value[16];
    hires_clock::time_point vcheck = hires_clock::now();
    while (!global->terminate) {
        VSync vsync(2 * global->bpm / 60.0, &vcheck); // Wait between sending
        if (col.ic != global->cur_color.ic) {
            col.ic = global->cur_color.ic;
            // The color has changed, send it to the LED strip!
            snprintf(value, sizeof(value), "%d,%d,%d", col.r, col.g, col.b);
            send_message("rgb", value, api);
        }
    }

    send_message("relay/0", "0", api);
}
