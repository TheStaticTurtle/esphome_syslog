#include "syslog_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
/*
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
*/

#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <lwip/ip.h>
#define IPPROTO_UDP IP_PROTO_UDP
#endif

namespace esphome {
namespace syslog {

static const char *TAG = "syslog";

//https://github.com/arcao/Syslog/blob/master/src/Syslog.h#L37-44
//https://github.com/esphome/esphome/blob/5c86f332b269fd3e4bffcbdf3359a021419effdd/esphome/core/log.h#L19-26
static const uint8_t esphome_to_syslog_log_levels[] = {0, 3, 4, 6, 5, 7, 7, 7};

SyslogComponent::SyslogComponent() {
    this->settings_.client_id = App.get_name();
}

void SyslogComponent::setup() {

    /*
     * Older versions of socket::set_sockaddr() return bogus results
     * if trying to log to a Legacy IP address when IPv6 is enabled.
     * Fixed by https://github.com/esphome/esphome/pull/7196
     */
    this->server_socklen = 0;
    if (ESPHOME_VERSION_CODE >= VERSION_CODE(2024, 8, 0)) {
        this->server_socklen = socket::set_sockaddr((struct sockaddr *)&this->server, sizeof(this->server),
                                                    this->settings_.address, this->settings_.port);
    }
#if USE_NETWORK_IPV6
    else if (this->settings_.address.find(':') != std::string::npos) {
        auto *server6 = reinterpret_cast<sockaddr_in6 *>(&this->server);
        memset(server6, 0, sizeof(*server6));
        server6->sin6_family = AF_INET6;
        server6->sin6_port = htons(this->settings_.port);

        ip6_addr_t ip6;
        inet6_aton(this->settings_.address.c_str(), &ip6);
        memcpy(server6->sin6_addr.un.u32_addr, ip6.addr, sizeof(ip6.addr));
        this->server_socklen = sizeof(*server6);
    }
#endif /* USE_NETWORK_IPV6 */
    else {
        auto *server4 = reinterpret_cast<sockaddr_in *>(&this->server);
        memset(server4, 0, sizeof(*server4));
        server4->sin_family = AF_INET;
        server4->sin_addr.s_addr = inet_addr(this->settings_.address.c_str());
        server4->sin_port = htons(this->settings_.port);
        this->server_socklen = sizeof(*server4);
    }
    if (!this->server_socklen) {
        ESP_LOGE(TAG, "Failed to parse server IP address '%s'", this->settings_.address.c_str());
        this->mark_failed();
        return;
    }
    this->socket_ = socket::socket(this->server.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (!this->socket_) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        this->mark_failed();
        return;
    }

    this->log(ESPHOME_LOG_LEVEL_INFO , "syslog", "Syslog started");
    ESP_LOGI(TAG, "Started");

    #ifdef USE_LOGGER
    if (logger::global_logger != nullptr) {
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
            if(!this->enable_logger || (level > this->settings_.min_log_level)) return;
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

    if (this->is_failed())
        return;

    level = level > 7 ? 7 : level;

    if (!this->socket_) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but socket isn't connected", tag.c_str(), payload.c_str(), level);
        return;
    }

    int pri = esphome_to_syslog_log_levels[level];
    std::string buf = str_sprintf("<%d>1 - %s %s - - - \xEF\xBB\xBF%s",
                                  pri, this->settings_.client_id.c_str(),
                                  tag.c_str(), payload.c_str());
    if (this->socket_->sendto(buf.c_str(), buf.length(), 0, (struct sockaddr *)&this->server, this->server_socklen) < 0) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but but failed for an unknown reason", tag.c_str(), payload.c_str(), level);
    }
}

float SyslogComponent::get_setup_priority() const {
    return setup_priority::AFTER_WIFI;
}

}  // namespace syslog
}  // namespace esphome
