// components/syslog/syslog_component.h

#pragma once
#ifndef SYSLOG_COMPONENT_H_0504CB6C_15D8_4AB4_A04C_8AF9063B737F
#define SYSLOG_COMPONENT_H_0504CB6C_15D8_4AB4_A04C_8AF9063B737F

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/text/text.h"
#include <unordered_map>
#include <set>

namespace esphome {
namespace syslog {

/**
 * @brief Defines the source of a log message
 */
enum class LogSource {
    LOGGER,      // Messages from the ESPHome logger system
    DIRECT,      // Messages from direct API calls (syslog.log action or lambda)
    INTERNAL     // Messages from the syslog component itself
};

/**
 * @brief Settings structure for Syslog configuration
 */
struct SyslogSettings {
    std::string address;      // IP address of the syslog server
    uint16_t port;            // Port of the syslog server
    std::string client_id;    // Client identifier to include in syslog messages
    int min_log_level;        // Minimum log level to forward
};

/**
 * @brief Component for sending logs to a Syslog server
 */
class SyslogComponent : public Component {
    public:
        SyslogComponent();

        float get_setup_priority() const override;

        void setup() override;
        void loop() override;
        
        // Runtime changeable settings
        void set_server_ip(const std::string &address);
        const std::string &get_server_ip() const { return this->settings_.address; }
        
        void set_server_port(uint16_t port);
        uint16_t get_server_port() const { return this->settings_.port; }
        
        void set_client_id(const std::string &client_id);
        const std::string &get_client_id() const { return this->settings_.client_id; }
        
        void set_min_log_level(int log_level) { this->settings_.min_log_level = log_level; }
        int get_min_log_level() const { return this->settings_.min_log_level; }
        
        void set_enable_logger_messages(bool en);
        bool get_enable_logger_messages() const { return this->enable_logger; }
        
        void set_strip_colors(bool strip_colors);
        bool get_strip_colors() const { return this->strip_colors; }

        // Component state controls
        void set_enable_direct_logs(bool en);
        bool get_enable_direct_logs() const { return this->enable_direct_logs; }
        
        void set_globally_enabled(bool en);
        bool get_globally_enabled() const { return this->globally_enabled; }
        bool is_setup() const { return this->socket_ != nullptr; }

        // Log source prefixing
        void set_direct_log_prefix(const std::string &prefix);
        const std::string &get_direct_log_prefix() const { return this->direct_log_prefix; }
        
        void set_logger_log_prefix(const std::string &prefix);
        const std::string &get_logger_log_prefix() const { return this->logger_log_prefix; }

        // Filter management
        void set_filter_mode(bool include_mode) { this->filter_include_mode = include_mode; }
        bool get_filter_mode() const { return this->filter_include_mode; }
        
        void clear_filters();
        void add_filter(const std::string &tag);
        void remove_filter(const std::string &tag);
        bool has_filter(const std::string &tag) const;
        std::vector<std::string> get_filters() const;
        
        // Filter string methods (comma-separated list)
        void set_filter_string(const std::string &filter_string);
        const std::string &get_filter_string() const { return this->filter_string; }
        
        // Register a text sensor for the filter string
        void register_filter_string_text(text::Text *text) {
            this->filter_string_text_ = text;
            // Set initial value
            if (this->filter_string_text_ != nullptr) {
                this->filter_string_text_->publish_state(this->filter_string);
            }
        }
        
        // Main logging function
        void log(uint8_t level, const std::string &tag, const std::string &payload, LogSource source = LogSource::DIRECT);
        LogSource get_message_source(const std::string &tag) const;
        
        // Helper method to extract component name from the tag
        static std::string extract_component_name(const std::string &tag);
        
        // Method to check if a tag should be filtered
        bool should_send_log(const std::string &tag);

    protected:
        bool strip_colors;                    // Whether to strip color codes from logger messages
        bool enable_logger;                   // Enable capturing from ESPHome logger
        bool enable_direct_logs;              // Enable direct API logging calls
        bool globally_enabled;                // Global on/off switch for the component
        bool filter_include_mode;             // Filter mode: true=include, false=exclude
        std::set<std::string> tag_filters;    // Set of tags to filter
        std::string filter_string;            // Original comma-separated filter string
        text::Text *filter_string_text_ = nullptr;  // Text sensor for filter string
        SyslogSettings settings_;             // Connection settings
        std::unique_ptr<socket::Socket> socket_ = nullptr;  // UDP socket
        struct sockaddr_storage server;       // Server address
        socklen_t server_socklen;             // Server address length
        
        // Prefix settings for different log sources
        std::string direct_log_prefix;        // Prefix for direct logs
        std::string logger_log_prefix;        // Prefix for logger messages
};

/**
 * @brief Action to send a log message
 */
template<typename... Ts> class SyslogLogAction : public Action<Ts...> {
    public:
        explicit SyslogLogAction(SyslogComponent *parent) : parent_(parent) {}
        TEMPLATABLE_VALUE(uint8_t, level)
        TEMPLATABLE_VALUE(std::string, tag)
        TEMPLATABLE_VALUE(std::string, payload)

        void play(Ts... x) override {
            this->parent_->log(this->level_.value(x...), this->tag_.value(x...), this->payload_.value(x...), LogSource::DIRECT);
        }

    protected:
        SyslogComponent *parent_;
};

/**
 * @brief Action to add a tag filter
 */
template<typename... Ts> class SyslogAddFilterAction : public Action<Ts...> {
public:
    explicit SyslogAddFilterAction(SyslogComponent *parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(std::string, tag)
    void play(Ts... x) override {
        this->parent_->add_filter(this->tag_.value(x...));
    }
protected:
    SyslogComponent *parent_;
};

/**
 * @brief Action to remove a tag filter
 */
template<typename... Ts> class SyslogRemoveFilterAction : public Action<Ts...> {
public:
    explicit SyslogRemoveFilterAction(SyslogComponent *parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(std::string, tag)
    void play(Ts... x) override {
        this->parent_->remove_filter(this->tag_.value(x...));
    }
protected:
    SyslogComponent *parent_;
};

/**
 * @brief Action to clear all filters
 */
template<typename... Ts> class SyslogClearFiltersAction : public Action<Ts...> {
public:
    explicit SyslogClearFiltersAction(SyslogComponent *parent) : parent_(parent) {}
    void play(Ts... x) override {
        this->parent_->clear_filters();
    }
protected:
    SyslogComponent *parent_;
};

/**
 * @brief Action to set filter string
 */
template<typename... Ts> class SyslogSetFilterStringAction : public Action<Ts...> {
public:
    explicit SyslogSetFilterStringAction(SyslogComponent *parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(std::string, filter_string)
    void play(Ts... x) override {
        this->parent_->set_filter_string(this->filter_string_.value(x...));
    }
protected:
    SyslogComponent *parent_;
};

}  // namespace syslog
}  // namespace esphome
#endif
