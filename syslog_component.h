#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include <Syslog.h>
#include <Udp.h>
#include <WiFiUdp.h>

namespace esphome {
namespace syslog {

struct SYSLOGSettings {
    std::string address;
    uint16_t port;
    std::string client_id;
};

//class UDP;

class SyslogComponent : public Component  {
    public:
        SyslogComponent();

        float get_setup_priority() const override;

        void setup() override;
        void loop() override;

        void set_server_ip(const std::string &address)   { this->settings_.address   = address; }
        void set_server_port(uint16_t port)              { this->settings_.port      = port; }
        void set_client_id(const std::string &client_id) { this->settings_.client_id = client_id; }

        void set_strip_colors(bool strip_colors) { this->strip_colors = strip_colors; }

        void log(uint8_t level, const std::string &tag, const std::string &payload);
    protected:
        bool strip_colors;
        SYSLOGSettings settings_;
        UDP *udp_;
};

template<typename... Ts> class SyslogLogAction : public Action<Ts...> {
    public:
        SyslogLogAction(SyslogComponent *parent) : parent_(parent) {}
        TEMPLATABLE_VALUE(uint8_t, level)
        TEMPLATABLE_VALUE(std::string, tag)
        TEMPLATABLE_VALUE(std::string, payload)

        void play(Ts... x) override {
            this->parent_->log(this->level_.value(x...), this->tag_.value(x...), this->payload_.value(x...));
        }

    protected:
        SyslogComponent *parent_;
};

}  // namespace syslog
}  // namespace esphome