// components/syslog/syslog_component.cpp

#include "syslog_component.h"

#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include <algorithm>  // for std::transform

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#ifdef USE_SOCKET_IMPL_LWIP_TCP
#include <lwip/ip.h>
#define IPPROTO_UDP IP_PROTO_UDP
#endif

namespace esphome {
namespace syslog {

static const char *TAG = "syslog";

// Map ESPHome log levels to syslog priorities
// https://github.com/arcao/Syslog/blob/master/src/Syslog.h#L37-44
// https://github.com/esphome/esphome/blob/5c86f332b269fd3e4bffcbdf3359a021419effdd/esphome/core/log.h#L19-26
static const uint8_t esphome_to_syslog_log_levels[] = {0, 3, 4, 6, 5, 7, 7, 7};

// Helper function to trim whitespace
static std::string trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Helper function to replace spaces with underscores
static std::string replace_spaces_with_underscores(const std::string &str) {
    std::string result = str;
    std::replace(result.begin(), result.end(), ' ', '_');
    return result;
}

// Helper function to ensure prefix ends with ": "
static std::string normalize_prefix(const std::string &prefix) {
    if (prefix.empty()) {
        return prefix;
    }
    
    std::string result = trim(prefix);
    
    // Replace any spaces with underscores
    result = replace_spaces_with_underscores(result);
    
    // Check if it ends with colon and space
    if (result.size() >= 2 && result.substr(result.size() - 2) == ": ") {
        return result;
    }
    
    // Check if it ends with just a colon
    if (!result.empty() && result.back() == ':') {
        return result + " ";
    }
    
    // Otherwise add the colon and space
    return result + ": ";
}

SyslogComponent::SyslogComponent() {
    this->settings_.client_id = App.get_name();
    this->filter_include_mode = false; // Default to exclude mode
    this->strip_colors = true;
    this->enable_logger = true;
    this->enable_direct_logs = true;    // Enable direct logging by default
    this->globally_enabled = true;      // Enable component by default
    this->filter_string = "";           // Initialize empty filter string
    this->direct_log_prefix = "";       // Initialize empty direct log prefix
    this->logger_log_prefix = "";       // Initialize empty logger log prefix
}

void SyslogComponent::setup() {
    // If component is globally disabled, don't set up the socket
    if (!this->globally_enabled) {
        this->log(ESPHOME_LOG_LEVEL_INFO, TAG, "Syslog component is disabled, skipping setup", LogSource::INTERNAL);
        return;
    }

    // Close existing socket if it exists
    if (this->socket_) {
        this->socket_.reset();
    }

    // Set up the server address structure
    this->server_socklen = 0;
    
    // Use the version-appropriate method for socket address setup
    if (ESPHOME_VERSION_CODE >= VERSION_CODE(2024, 8, 0)) {
        // Use the new method for ESPHome 2024.8.0 and later
        this->server_socklen = socket::set_sockaddr((struct sockaddr *)&this->server, sizeof(this->server),
                                                this->settings_.address, this->settings_.port);
    }
#if USE_NETWORK_IPV6
    else if (this->settings_.address.find(':') != std::string::npos) {
        // IPv6 address handling for older ESPHome versions
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
        // IPv4 address handling for older ESPHome versions
        auto *server4 = reinterpret_cast<sockaddr_in *>(&this->server);
        memset(server4, 0, sizeof(*server4));
        server4->sin_family = AF_INET;
        server4->sin_addr.s_addr = inet_addr(this->settings_.address.c_str());
        server4->sin_port = htons(this->settings_.port);
        this->server_socklen = sizeof(*server4);
    }
    
    // Check if we successfully set up the server address
    if (!this->server_socklen) {
        this->log(ESPHOME_LOG_LEVEL_ERROR, TAG, 
                 "Failed to parse server IP address '" + this->settings_.address + "'", 
                 LogSource::INTERNAL);
        this->mark_failed();
        return;
    }
    
    // Create UDP socket
    this->socket_ = socket::socket(this->server.ss_family, SOCK_DGRAM, IPPROTO_UDP);
    if (!this->socket_) {
        this->log(ESPHOME_LOG_LEVEL_ERROR, TAG, "Failed to create UDP socket", LogSource::INTERNAL);
        this->mark_failed();
        return;
    }
 
    // Log successful startup
    this->log(ESPHOME_LOG_LEVEL_INFO, TAG, "------------------------ Syslog started ------------------------", LogSource::INTERNAL);
    this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
              "Started with server: " + this->settings_.address + " -> " + std::to_string(this->settings_.port), 
              LogSource::INTERNAL);
    
    // Set up logger callback if logger is available
    #ifdef USE_LOGGER
    if (logger::global_logger != nullptr && this->enable_logger) {
        logger::global_logger->add_on_log_callback([this](int level, const char *tag, const char *message) {
            // Skip if component is disabled or level is filtered
            if (!this->globally_enabled || (level > this->settings_.min_log_level)) 
                return;
            
            // Check if tag is filtered
            std::string tag_str(tag);
            if (!this->should_send_log(tag_str)) {
                return;
            }
            
            // Forward the log message, stripping color codes if configured
            if (this->strip_colors) {
                // Strip the ESPHome color codes:
                // 033[0;xxx at beginning and 033[0m at end
                std::string org_msg(message);
                if (org_msg.size() > 11) { // Ensure message is long enough to have color codes
                    this->log(level, tag, org_msg.substr(7, org_msg.size() - 7 - 4), LogSource::LOGGER);
                } else {
                    // Message too short to have color codes, send as is
                    this->log(level, tag, message, LogSource::LOGGER);
                }
            } else {
                this->log(level, tag, message, LogSource::LOGGER);
            }
        });
    }
    #endif
}

void SyslogComponent::loop() {
    // Currently nothing to do in loop
}

void SyslogComponent::set_server_ip(const std::string &address) {
    if (this->settings_.address != address) {
        // Store the new address
        std::string old_address = this->settings_.address;
        this->settings_.address = address;
        
        // Only attempt to recreate the socket if we're already set up
        if (this->globally_enabled && this->is_setup()) {
            // Recreate the socket with new address
            this->setup();
            
            // Log the change
            this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                     "Syslog server IP updated: " + old_address + " -> " + address, 
                     LogSource::INTERNAL);
        }
    }
}

void SyslogComponent::set_server_port(uint16_t port) {
    if (this->settings_.port != port) {
        // Store the old port for logging
        uint16_t old_port = this->settings_.port;
        this->settings_.port = port;
        
        // Only attempt to recreate the socket if we're already set up
        if (this->globally_enabled && this->is_setup()) {
            // Recreate the socket with new port
            this->setup();
            
            // Log the change
            this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                     "Syslog server port updated: " + std::to_string(old_port) + " -> " + 
                     std::to_string(port),
                     LogSource::INTERNAL);
        }
    }
}

void SyslogComponent::set_enable_logger_messages(bool en) {
    if (this->enable_logger != en) { 
        this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                 "Logger messages: " + std::string(this->enable_logger ? "enabled" : "disabled") + 
                 " -> " + std::string(en ? "enabled" : "disabled"),
                 LogSource::INTERNAL);
        this->enable_logger = en;
    }
}

void SyslogComponent::set_strip_colors(bool strip_colors) {
    if (this->strip_colors != strip_colors) {    
        this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                 "Strip colors: " + std::string(this->strip_colors ? "enabled" : "disabled") + 
                 " -> " + std::string(strip_colors ? "enabled" : "disabled"),
                 LogSource::INTERNAL);
        this->strip_colors = strip_colors;
    }
}

void SyslogComponent::set_enable_direct_logs(bool en) {
    if (this->enable_direct_logs != en) {        
        this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                 "Direct logging: " + std::string(this->enable_direct_logs ? "enabled" : "disabled") + 
                 " -> " + std::string(en ? "enabled" : "disabled"),
                 LogSource::INTERNAL);
        this->enable_direct_logs = en;
    }
}

void SyslogComponent::set_globally_enabled(bool en) {
    if (this->globally_enabled != en) {
        this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                 "Syslog component: " + std::string(this->globally_enabled ? "enabled" : "disabled") + 
                 " -> " + std::string(en ? "enabled" : "disabled"),
                 LogSource::INTERNAL);
        
        this->globally_enabled = en;
        
        // If enabling, make sure to set up the socket
        if (en) {
            this->setup();
        } else {
            // If disabling, close any open socket
            if (this->socket_) {
                this->socket_.reset();
            }
        }
    }
}

void SyslogComponent::add_filter(const std::string &tag) {
    this->tag_filters.insert(tag);
    this->log(ESPHOME_LOG_LEVEL_INFO, TAG, "Added filter for tag: '" + tag + "'", LogSource::INTERNAL);
}

void SyslogComponent::remove_filter(const std::string &tag) {
    this->tag_filters.erase(tag);
    this->log(ESPHOME_LOG_LEVEL_INFO, TAG, "Removed filter for tag: '" + tag + "'", LogSource::INTERNAL);
}

void SyslogComponent::clear_filters() {
    this->tag_filters.clear();
    this->filter_string = "";
    
    // Update text sensor if available
    if (this->filter_string_text_ != nullptr) {
        this->filter_string_text_->publish_state("");
    }
    
    this->log(ESPHOME_LOG_LEVEL_INFO, TAG, "All filters cleared", LogSource::INTERNAL);
}

void SyslogComponent::set_client_id(const std::string &client_id) {
    // Replace spaces with underscores for client_id
    this->settings_.client_id = replace_spaces_with_underscores(client_id);
}

void SyslogComponent::set_direct_log_prefix(const std::string &prefix) {
    this->direct_log_prefix = normalize_prefix(prefix);
}

void SyslogComponent::set_logger_log_prefix(const std::string &prefix) {
    this->logger_log_prefix = normalize_prefix(prefix);
}

void SyslogComponent::set_filter_string(const std::string &filter_string) {
    if (this->filter_string != filter_string) {
        this->filter_string = filter_string;
        
        // Clear existing filters
        this->tag_filters.clear();
        
        // Parse the new filter string (comma-separated list)
        if (!filter_string.empty()) {
            size_t start = 0;
            size_t end = 0;
            
            while ((end = filter_string.find(',', start)) != std::string::npos) {
                std::string item = trim(filter_string.substr(start, end - start));
                
                if (!item.empty()) {
                    this->add_filter(item);
                }
                
                start = end + 1;
            }
            
            // Add the last item
            std::string item = trim(filter_string.substr(start));
            
            if (!item.empty()) {
                this->add_filter(item);
            }
        }
        
        // Update text sensor if available
        if (this->filter_string_text_ != nullptr) {
            this->filter_string_text_->publish_state(this->filter_string);
        }
        
        this->log(ESPHOME_LOG_LEVEL_INFO, TAG, 
                 "Filter string updated: '" + filter_string + "'", 
                 LogSource::INTERNAL);
    }
}

bool SyslogComponent::has_filter(const std::string &tag) const {
    return this->tag_filters.find(tag) != this->tag_filters.end();
}

std::vector<std::string> SyslogComponent::get_filters() const {
    std::vector<std::string> result;
    result.reserve(this->tag_filters.size());  // Optimize by pre-allocating
    for (const auto &tag : this->tag_filters) {
        result.push_back(tag);
    }
    return result;
}

std::string SyslogComponent::extract_component_name(const std::string &tag) {
    size_t colon_pos = tag.find(':');
    if (colon_pos != std::string::npos) {
        return tag.substr(0, colon_pos);
    }
    return tag;
}

bool SyslogComponent::should_send_log(const std::string &tag) {
    // Extract component name from tag (before the colon)
    std::string component = extract_component_name(tag);

    // Create a lower-case copy of the filter string for case-insensitive "all" check
    std::string filter_lower = this->filter_string;
    std::transform(filter_lower.begin(), filter_lower.end(), filter_lower.begin(), ::tolower);

    if (this->filter_include_mode) {
        // In Include Mode:
        // - "all" means include everything.
        // - Empty filter means include nothing.
        if (filter_lower == "all") {
            return true;
        }
        if (this->filter_string.empty()) {
            return false;
        }
        // Otherwise, include only if the component is in the filter list.
        return this->has_filter(component);
    } else {
        // In Exclude Mode:
        // - "all" means exclude everything.
        // - Empty filter means include everything.
        if (filter_lower == "all") {
            return false;
        }
        if (this->filter_string.empty()) {
            return true;
        }
        // Otherwise, exclude the log if the component is in the filter list.
        return !this->has_filter(component);
    }
}

LogSource SyslogComponent::get_message_source(const std::string &tag) const {
    // Check if tag starts with direct log prefix (if set)
    if (!this->direct_log_prefix.empty() && 
        tag.substr(0, this->direct_log_prefix.length()) == this->direct_log_prefix) {
        return LogSource::DIRECT;
    }
    
    // Check if tag starts with logger log prefix (if set)
    if (!this->logger_log_prefix.empty() && 
        tag.substr(0, this->logger_log_prefix.length()) == this->logger_log_prefix) {
        return LogSource::LOGGER;
    }
    
    // If tag is "syslog", it's an internal message
    if (tag == "syslog") {
        return LogSource::INTERNAL;
    }
    
    // Otherwise assume it's a direct log
    return LogSource::DIRECT;
}

void SyslogComponent::log(uint8_t level, const std::string &tag, const std::string &payload, LogSource source) {
    // Check if component is enabled
    if (!this->globally_enabled || this->is_failed()) {
        return;
    }
     
    // For direct log calls, check the enable_direct_logs flag
    if (source == LogSource::DIRECT && !this->enable_direct_logs && tag != "syslog") {
        return;
    }
    
    // Add this new check for logger messages
    if (source == LogSource::LOGGER && !this->enable_logger) {
        return;
    }

    // Ensure level is valid
    level = std::min(level, static_cast<uint8_t>(7));
    
    // Check if socket is available
    if (!this->socket_) {
        ESP_LOGW(TAG, "Tried to send \"%s\"@\"%s\" with level %d but socket isn't connected", 
                tag.c_str(), payload.c_str(), level);
        return;
    }
    
    // Apply prefixes based on source if configured
    std::string modified_tag = tag;
    
    // Add source prefix if configured
    if (source == LogSource::DIRECT && !this->direct_log_prefix.empty()) {
        // Only add the prefix if it's not already there (avoids duplication on log actions)
        if (modified_tag.substr(0, this->direct_log_prefix.length()) != this->direct_log_prefix) {
            modified_tag = this->direct_log_prefix + modified_tag;
        }
    } else if (source == LogSource::LOGGER && !this->logger_log_prefix.empty()) {
        // Only add the prefix if it's not already there
        if (modified_tag.substr(0, this->logger_log_prefix.length()) != this->logger_log_prefix) {
            modified_tag = this->logger_log_prefix + modified_tag;
        }
    }
    
    // Format according to syslog protocol
    int pri = esphome_to_syslog_log_levels[level];
    std::string buf = str_sprintf("<%d>1 - %s %s - - - \xEF\xBB\xBF%s",
                                 pri, this->settings_.client_id.c_str(),
                                 modified_tag.c_str(), payload.c_str());
    
    // Send the message
    if (this->socket_->sendto(buf.c_str(), buf.length(), 0, 
                              (struct sockaddr *)&this->server, this->server_socklen) < 0) {
        ESP_LOGW(TAG, "Failed to send syslog message: \"%s\"@\"%s\"", 
                modified_tag.c_str(), payload.c_str());
    }
}

float SyslogComponent::get_setup_priority() const {
    return setup_priority::AFTER_WIFI;
}

}  // namespace syslog
}  // namespace esphome
