# ESPHome Syslog Component

A powerful and configurable Syslog component for ESPHome that forwards logs to a Syslog server. The component can automatically attach itself to the ESPHome logger core module (similar to how the MQTT component works with `log_topic`) while providing advanced filtering and configuration options.

## Features

- Forward ESPHome logs to a Syslog server
- Direct logging through automations
- Advanced filtering (include/exclude specific components)
- Configurable log levels
- Color code stripping
- Runtime reconfigurable (server IP, port, filters)
- Support for both IPv4 and IPv6 addresses
- Component-specific log prefixing
- Integration with Text components for filter management

## Installation

### Using External Components (Recommended)

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/TheStaticTurtle/esphome_syslog
      ref: main  # or a specific commit SHA
    refresh: always
```

### Manual Installation

To install manually, locate your `esphome` folder, create a folder named `custom_components` (if it doesn't exist), navigate into it, and execute:

```shell
git clone https://github.com/TheStaticTurtle/esphome_syslog.git
mv esphome_syslog/components/syslog .
rm -rf esphome_syslog
```

## Basic Configuration

The simplest configuration only requires adding the component to your ESPHome configuration:

```yaml
syslog:
```

With this minimal configuration, the component will **broadcast logs to everyone on the network** using the default UDP port 514.

## Configuration Options

| Option                | Type      | Default           | Description                                                       |
|-----------------------|-----------|-------------------|-------------------------------------------------------------------|
| `ip_address`          | string    | "255.255.255.255" | IP address of the Syslog server (IPv4 or IPv6)                   |
| `port`                | integer   | 514               | UDP port of the Syslog server                                     |
| `client_id`           | string    | Device name       | Client identifier in Syslog messages                             |
| `strip_colors`        | boolean   | true              | Remove ESPHome color codes from log messages                      |
| `enable_logger`       | boolean   | true              | Enable forwarding of ESPHome logger messages                      |
| `enable_direct_logs`  | boolean   | true              | Enable direct logging through automations                         |
| `globally_enabled`    | boolean   | true              | Global switch to enable/disable the component                     |
| `min_level`           | string    | "DEBUG"           | Minimum log level to forward (ERROR, WARN, INFO, DEBUG, etc.)     |
| `filter_mode`         | string/bool | "exclude"       | Filter mode: "include" or "exclude" (or true/false)               |
| `filters`             | list      | []                | List of component tags to include/exclude                         |
| `filter_string`       | string    | ""                | Comma-separated list of component tags to include/exclude         |
| `filter_text`         | id        | -                 | Text component ID to display/update filter string                 |
| `direct_log_prefix`   | string    | ""                | Prefix added to direct log messages                               |
| `logger_log_prefix`   | string    | ""                | Prefix added to logger messages                                   |

## Configuration Options

### Logging Configuration

The following options allow you to customize how log messages are displayed:

- `client_id`: String that sets the device name used in Syslog messages. This also controls the name of the log file created by the Syslog server.
  - Example: When set to `"ESP32 C3"`, logs will show this identifier and be saved to `syslog-ESP32_C3.log`

- `direct_log_prefix`: String that is added as a prefix to direct log messages.
  - Example: When set to `"direct"`, direct log messages will be prefixed with this text

- `logger_log_prefix`: String that is added as a prefix to logger messages.
  - Example: When set to `"logger"`, logger messages will be prefixed with this text

### Example Log Output

When configured with:
```yaml
client_id: "ESP32_C3"
direct_log_prefix: "direct"
logger_log_prefix: "logger"
```

The logs will appear as:

```
Apr  8 18:30:11 ESP32_C3 direct:[[Boot]] - => Device booted
Apr  8 18:30:11 ESP32_C3 logger:[sensor] - ﻿[D][sensor:093]: 'WiFi Signal': Sending state -64.00000 dBm with 0 decimals of accuracy
```

Note that the `client_id` appears after the timestamp, and the prefixes are added to the beginning of the actual log message content.

## Advanced Configuration Examples

### Server Configuration with Port and Client ID

```yaml
syslog:
  ip_address: "192.168.1.53"
  port: 514
  client_id: "living_room_esp32"
```

### Filtering by Log Level and Components

```yaml
syslog:
  ip_address: "192.168.1.53"
  min_level: INFO
  filter_mode: exclude
  filters:
    - wifi
    - mqtt
    - api
```

This configuration:
- Only forwards logs of level INFO or higher
- Excludes logs from the wifi, mqtt, and api components

### Using a Text Component for Filter Management

This allows runtime management of filters through Home Assistant or other frontends:

```yaml
text:
  - platform: template
    id: syslog_filter
    name: "Syslog Filter"
    mode: text
    restore_value: true
    
syslog:
  filter_mode: include
  filter_text: syslog_filter
```

### Full Control with Switches and UI Integration

```yaml
# Syslog configuration with filter text
syslog:
  id: syslog_component
  filter_text: syslog_filter_text
  
# Text component for filter management
text:
  - platform: template
    name: "Filter Input"
    id: syslog_filter_text
    entity_category: config
    optimistic: true
    restore_value: true
    mode: text
    on_value:
      then:
        - syslog.set_filter_string:
            filter_string: !lambda 'return x;'

# Text component for server IP
text:
  - platform: template
    name: "Server IP"
    id: syslog_server_ip
    entity_category: config
    mode: text
    optimistic: true
    restore_value: true
    initial_value: "192.168.1.100"
    on_value:
      - lambda: 'id(syslog_component).set_server_ip(x);'

# Number component for port
number:
  - platform: template
    name: "Port"
    id: syslog_server_port
    entity_category: config
    min_value: 1
    max_value: 65535
    step: 1
    initial_value: 514
    restore_value: true
    optimistic: true
    mode: box
    on_value:
      then:
        - lambda: 'id(syslog_component).set_server_port(x);'

# Main enable/disable switch
switch:
  - platform: template
    name: "Syslog Enable"
    id: syslog_enable_switch
    entity_category: config
    optimistic: true
    restore_mode: RESTORE_DEFAULT_ON
    initial_value: true
    on_turn_on:
      - lambda: 'id(syslog_component).set_globally_enabled(true);'
    on_turn_off:
      - lambda: 'id(syslog_component).set_globally_enabled(false);'

# Filter mode switch
switch:
  - platform: template
    id: syslog_filter_mode
    name: "Filter Mode (ON=Include/OFF=Exclude)"
    entity_category: config
    optimistic: true
    restore_mode: RESTORE_DEFAULT_OFF
    on_turn_on:
      - lambda: 'id(syslog_component).set_filter_mode(true);'  # Include mode
    on_turn_off:
      - lambda: 'id(syslog_component).set_filter_mode(false);'  # Exclude mode
```

### Log Level Selection with Dropdown

```yaml
select:
  - platform: template
    id: syslog_log_level
    name: "Log Level"
    entity_category: config
    optimistic: true
    options:
      - "NONE"
      - "ERROR"
      - "WARN" 
      - "INFO"
      - "CONFIG"
      - "DEBUG"
      - "VERBOSE"
      - "VERY_VERBOSE"
    initial_option: "DEBUG"
    on_value:
      - lambda: |-
          if (x == "NONE")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_NONE);
          else if (x == "ERROR")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_ERROR);
          else if (x == "WARN")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_WARN);
          else if (x == "INFO")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_INFO);
          else if (x == "CONFIG")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_CONFIG);
          else if (x == "DEBUG")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_DEBUG);
          else if (x == "VERBOSE")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_VERBOSE);
          else if (x == "VERY_VERBOSE")
            id(syslog_component).set_min_log_level(ESPHOME_LOG_LEVEL_VERY_VERBOSE);
```

### Saving and Restoring Filter Settings

This configuration automatically restores filter settings on boot:

```yaml
esphome:
  name: esp32_syslog_example
  on_boot:
    priority: -100
    then:
      - lambda: |-
          // Restore the filter string from the text sensor
          id(syslog_component).set_filter_string(id(syslog_filter_text).state);
```

## Automation Actions

The component provides several automation actions:

### Sending Custom Logs

```yaml
button:
  - platform: template
    name: "Test Syslog"
    on_press:
      - syslog.log:
          level: 6  # INFO level
          tag: "custom_action"
          payload: "Button pressed!"
```

### Managing Filters

```yaml
button:
  - platform: template
    name: "Add Filter"
    on_press:
      - syslog.add_filter:
          tag: "wifi"
          
  - platform: template
    name: "Remove Filter"
    on_press:
      - syslog.remove_filter:
          tag: "wifi"
          
  - platform: template
    name: "Clear Filters"
    on_press:
      - syslog.clear_filters:
      
  - platform: template
    name: "Set Filter String"
    on_press:
      - syslog.set_filter_string:
          filter_string: "wifi,mqtt,api"
```

### Logging Sensor Data

```yaml
interval:
  - interval: 5min
    then:
      - lambda: |-
          // Get current temperature and format message
          float temp = id(room_temperature).state;
          char message[64];
          sprintf(message, "Current temperature is %.1f°C", temp);
          
      - syslog.log:
          level: 6  # INFO level
          tag: "temp_monitor"
          payload: !lambda 'return message;'
```

### Logging Important Events

```yaml
binary_sensor:
  - platform: template
    name: "Temperature Rise Alert"
    lambda: |-
      static float last_temp = 0;
      float current_temp = id(room_temp).state;
      bool significant_change = abs(current_temp - last_temp) > 2.0;
      if (significant_change) {
        last_temp = current_temp;
      }
      return significant_change;
    on_press:
      - syslog.log:
          level: 4  # WARNING level
          tag: "temp_alert"
          payload: !lambda 'return "Temperature changed significantly to " + to_string(id(room_temp).state) + "°C";'
```

## Log Levels Mapping

Due to differences between ESPHome and Syslog log levels, the component maps them as follows:

| ESPHome Level              | Syslog Priority |
|----------------------------|-----------------|
| NONE                       | EMERG (0)       |
| ERROR                      | ERR (3)         |
| WARN                       | WARNING (4)     |
| INFO                       | INFO (6)        |
| CONFIG                     | NOTICE (5)      |
| DEBUG                      | DEBUG (7)       |
| VERBOSE                    | DEBUG (7)       |
| VERY_VERBOSE               | DEBUG (7)       |

## Example Log Output

When properly configured, you'll see log messages in your Syslog server like these:

```
Apr  7 12:34:56 esp32_living_room wifi: WiFi connected to 'MyNetwork'
Apr  7 12:34:57 esp32_living_room direct:button: Button pressed!
Apr  7 12:34:58 esp32_living_room logger:sensor: Temperature: 22.5°C
Apr  7 12:35:00 esp32_living_room syslog: Filter string updated: 'wifi,mqtt'
```

## Complete Example YAML

For a complete example configuration demonstrating all features of the Syslog component, please refer to the [example.yaml](https://github.com/TheStaticTurtle/esphome_syslog/blob/main/example.yaml) file in the repository.

## Compatibility

This component has been tested with:
- ESP8266
- ESP32

Note: The ESP32 may experience issues when sending many log messages in rapid succession, such as during boot when configuration is printed. See [issue #7 comment](https://github.com/TheStaticTurtle/esphome_syslog/issues/7#issuecomment-1236194816) for more details.

## Troubleshooting

If you're not seeing logs on your Syslog server:

1. Check that your server IP and port are correct
2. Ensure your network allows UDP traffic on the configured port
3. Verify that log levels and filters aren't blocking messages
4. Check if the component is globally enabled
5. For ESP32, try reducing the logging verbosity to prevent buffer overflow

## Contributing

Contributions are welcome! If you find a bug or want to add a feature, please open an issue or submit a pull request.

## License

This project is licensed under the ISC License - see the LICENSE file for details.