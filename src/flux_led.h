#pragma once

#include "color.h"
#include "global_state.h"
#include "led_strip.h"
#include "thread_sync.h"

#include <string>
#include <vector>

class FluxLed : public LedStrip {
    using message = std::vector<unsigned char>;

public:
    FluxLed(std::string host, GlobalState* state, std::shared_ptr<ThreadSync> ts);
    ~FluxLed();

    void startup() override;
    void shutdown() override;
    void set_rgb(Color c) override;

private:
    void connect();
    void close();
    bool send_msg(const message& bytes);
    bool read_msg(message& bytes, int expected);
    void determine_query_len();
    bool query_state(message& state);
    void update_state();
    void turn_on();
    void turn_off();

    std::string hostname; // Host name of the LED strip
    std::string resolved; // Resolved IP address

    enum Protocol { PROT_UNKNOWN, PROT_LEDENET_ORIGINAL, PROT_LEDENET };

    enum Mode { MODE_UNKNOWN, MODE_COLOR, MODE_WW, MODE_CUSTOM, MODE_PRESET, MODE_TIMER };

    int port { 5577 };
    int timeout { 5 };

    Protocol protocol { PROT_UNKNOWN };
    bool rgbwcapable { false };
    bool rgbwprotocol { false };

    message raw_state;
    bool _is_on { false };
    Mode _mode { MODE_UNKNOWN };
    int _socket { 0 };
    unsigned int _query_len { 0 };
    bool _use_csum { true };
};
