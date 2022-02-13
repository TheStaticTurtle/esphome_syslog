#include "syslog_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
/*
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"
*/

namespace esphome {
namespace syslog {

static const char *TAG = "syslog";

//https://github.com/arcao/Syslog/blob/master/src/Syslog.h#L37-44
//https://github.com/esphome/esphome/blob/5c86f332b269fd3e4bffcbdf3359a021419effdd/esphome/core/log.h#L19-26
static const uint8_t esphome_to_syslog_log_levels[] = {0, 3, 4, 6, 5, 7, 7, 7};

SyslogComponent::SyslogComponent() {
    //global_syslog = this;
    this->settings_.client_id = App.get_name();
}

void SyslogComponent::setup() {
    this->udp_ = new WiFiUDP();
    this->log(ESPHOME_LOG_LEVEL_INFO , "syslog", "Syslog started");
    ESP_LOGI(TAG, "Started");

    #ifdef USE_LOGGER
    if (logger::global_logger != nullptr) {
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
            if(!this->enable_logger ||
	       (level > this->settings_.min_log_level)) return;
            if(this->strip_colors) { //Strips the "033[0;xxx" at the beginning and the "#033[0m" at the end of log messages
                std::string org_msg(message);
                this->log(level, tag, org_msg.substr(7, org_msg.size() -7 -4));
            } else {
                this->log(level, tag, message);
            }
        });
    }
    #endif
}

void SyslogComponent::loop() {
}

void SyslogComponent::log(uint8_t level, const std::string &tag, const std::string &payload) {
    level = level > 7 ? 7 : level;

    Syslog syslog(
        *this->udp_,
        this->settings_.address.c_str(),
        this->settings_.port,
        this->settings_.client_id.c_str(),
        tag.c_str(),
        LOG_KERN
    );
    syslog.log(esphome_to_syslog_log_levels[level],  payload.c_str());
}

float SyslogComponent::get_setup_priority() const {
    return setup_priority::LATE;
}

}  // namespace syslog
}  // namespace esphome
