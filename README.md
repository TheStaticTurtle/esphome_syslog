# esphome_syslog

A simple syslog component for esphome. The component is designed to auto attach itself to the logger core module (like the mqtt component does with the `log_topic`)

## How to

To install, locate your `esphome` foler, create a folder named `custom_components` got into it and execute 
```shell
git clone https://github.com/TheStaticTurtle/esphome_syslog.git syslog
```

Simply add this to your configuration: 
```yaml
syslog:
```

When used like this, the component will simply **broadcast it's log to everyone on the network** to change this beahviour you can add the `ip_address` and `port` option like this:
```yaml
syslog:
    ip_address: "192.168.1.53"
    port: 514
```

Default beahviour strips the esphome color tags from the log (The `033[0;xxx` and the `#033[0m`) if you do not want this set the `strip_colors` option to False.

Due the differences in log levels of syslog and esphome I had to translate them, here is a table:
| Esphome level                  | Syslog level |
|--------------------------------|--------------|
| ESPHOME_LOG_LEVEL_NONE         | LOG_EMERG    |
| ESPHOME_LOG_LEVEL_ERROR        | LOG_ERR      |
| ESPHOME_LOG_LEVEL_WARN         | LOG_WARNING  |
| ESPHOME_LOG_LEVEL_INFO         | LOG_INFO     |
| ESPHOME_LOG_LEVEL_CONFIG       | LOG_NOTICE   |
| ESPHOME_LOG_LEVEL_DEBUG        | LOG_DEBUG    |
| ESPHOME_LOG_LEVEL_VERBOSE      | LOG_DEBUG    |
| ESPHOME_LOG_LEVEL_VERY_VERBOSE | LOG_DEBUG    |

This table is however open to discution as it's my interpretation, if you want to change it you can do so in the `syslog_component.cpp` file and change the array at line 22

## Warning
This component should not break anything however if it does please open a issue. I have successfuly tested this component with an esp8266 it is possible that it will not work on an esp32 but if it doesn't it should be really easy to add compatibility
