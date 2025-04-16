import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import automation
from esphome.const import (
    CONF_ID, 
    CONF_IP_ADDRESS, 
    CONF_PORT, 
    CONF_CLIENT_ID, 
    CONF_LEVEL, 
    CONF_PAYLOAD, 
    CONF_TAG,
    CONF_MODE
)
from esphome.components import logger, text

# Configuration constants
CONF_STRIP_COLORS = "strip_colors"
CONF_ENABLE_LOGGER_MESSAGES = "enable_logger"
CONF_ENABLE_DIRECT_LOGS = "enable_direct_logs"
CONF_GLOBALLY_ENABLED = "globally_enabled"
CONF_MIN_LEVEL = "min_level"
CONF_FILTER_MODE = "filter_mode"
CONF_INCLUDE = "include"
CONF_EXCLUDE = "exclude"
CONF_FILTERS = "filters"
CONF_FILTER_STRING = "filter_string"
CONF_FILTER_TEXT = "filter_text"
CONF_DIRECT_LOG_PREFIX = "direct_log_prefix"
CONF_LOGGER_LOG_PREFIX = "logger_log_prefix"

# Component dependencies
DEPENDENCIES = ['logger', 'network', 'socket']

# Namespace setup
syslog_ns = cg.esphome_ns.namespace('syslog')

# Component class definitions
SyslogComponent = syslog_ns.class_('SyslogComponent', cg.Component)
SyslogLogAction = syslog_ns.class_('SyslogLogAction', automation.Action)
SyslogAddFilterAction = syslog_ns.class_('SyslogAddFilterAction', automation.Action)
SyslogRemoveFilterAction = syslog_ns.class_('SyslogRemoveFilterAction', automation.Action)
SyslogClearFiltersAction = syslog_ns.class_('SyslogClearFiltersAction', automation.Action)
SyslogSetFilterStringAction = syslog_ns.class_('SyslogSetFilterStringAction', automation.Action)

# Define all log levels in uppercase for validation
LOG_LEVEL_OPTIONS = [level.upper() for level in logger.LOG_LEVELS]

# Validate filter mode
def validate_filter_mode(value):
    """
    Validates the filter mode setting, accepting string or boolean values.
    For strings: 'include' -> True, 'exclude' -> False
    For booleans: True = include mode, False = exclude mode
    """
    if isinstance(value, str):
        if value.lower() == "include":
            return True
        elif value.lower() == "exclude":
            return False
    if isinstance(value, bool):
        return value
    raise cv.Invalid(f"Filter mode must be either 'include', 'exclude', True (include), or False (exclude)")

# Custom validator for log levels that handles case insensitivity
def validate_log_level(value):
    """
    Validates log level values against ESPHome's defined log levels,
    handling case-insensitivity by converting to uppercase before validation.
    """
    if isinstance(value, str):
        upper_value = value.upper()
        if upper_value in LOG_LEVEL_OPTIONS:
            return upper_value
    raise cv.Invalid(f"Unknown log level '{value}', valid options are {', '.join(LOG_LEVEL_OPTIONS)}.")

# Main component configuration schema
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SyslogComponent),
    cv.Optional(CONF_IP_ADDRESS, default="255.255.255.255"): cv.string_strict,
    cv.Optional(CONF_PORT, default=514): cv.port,
    cv.Optional(CONF_CLIENT_ID): cv.string_strict,  # Optional client ID, defaults to device name
    cv.Optional(CONF_ENABLE_LOGGER_MESSAGES, default=True): cv.boolean,
    cv.Optional(CONF_ENABLE_DIRECT_LOGS, default=True): cv.boolean,
    cv.Optional(CONF_GLOBALLY_ENABLED, default=True): cv.boolean,
    cv.Optional(CONF_STRIP_COLORS, default=True): cv.boolean,
    cv.Optional(CONF_MIN_LEVEL, default="DEBUG"): validate_log_level,
    cv.Optional(CONF_FILTER_MODE, default="exclude"): validate_filter_mode,
    cv.Optional(CONF_FILTERS, default=[]): cv.ensure_list(cv.string),
    cv.Optional(CONF_FILTER_STRING, default=""): cv.string,
    cv.Optional(CONF_FILTER_TEXT): cv.use_id(text.Text),
    cv.Optional(CONF_DIRECT_LOG_PREFIX, default=""): cv.string,
    cv.Optional(CONF_LOGGER_LOG_PREFIX, default=""): cv.string,
})

# Action schemas
SYSLOG_LOG_ACTION_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SyslogComponent),
    cv.Required(CONF_LEVEL): cv.templatable(cv.int_range(min=0, max=7)),
    cv.Required(CONF_TAG): cv.templatable(cv.string),
    cv.Required(CONF_PAYLOAD): cv.templatable(cv.string),
})

SYSLOG_ADD_FILTER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SyslogComponent),
    cv.Required(CONF_TAG): cv.templatable(cv.string),
})

SYSLOG_REMOVE_FILTER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SyslogComponent),
    cv.Required(CONF_TAG): cv.templatable(cv.string),
})

SYSLOG_CLEAR_FILTERS_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SyslogComponent),
})

SYSLOG_SET_FILTER_STRING_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.use_id(SyslogComponent),
    cv.Required(CONF_FILTER_STRING): cv.templatable(cv.string),
})

def to_code(config):
    """
    Translates the YAML configuration to C++ code for the ESPHome runtime.
    Handles component setup and configuration of all parameters.
    """
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    
    # Configure basic settings
    cg.add(var.set_enable_logger_messages(config[CONF_ENABLE_LOGGER_MESSAGES]))
    cg.add(var.set_enable_direct_logs(config[CONF_ENABLE_DIRECT_LOGS]))
    cg.add(var.set_globally_enabled(config[CONF_GLOBALLY_ENABLED]))
    cg.add(var.set_strip_colors(config[CONF_STRIP_COLORS]))
    cg.add(var.set_server_ip(config[CONF_IP_ADDRESS]))
    cg.add(var.set_server_port(config[CONF_PORT]))
    
    # Set client ID if provided, otherwise defaults to device name
    if CONF_CLIENT_ID in config:
        cg.add(var.set_client_id(config[CONF_CLIENT_ID]))
    
    # Configure log level
    cg.add(var.set_min_log_level(logger.LOG_LEVELS[config[CONF_MIN_LEVEL]]))
    
    # Configure filter mode
    cg.add(var.set_filter_mode(config[CONF_FILTER_MODE]))
    
    # Configure log prefixes if provided
    if CONF_DIRECT_LOG_PREFIX in config:
        cg.add(var.set_direct_log_prefix(config[CONF_DIRECT_LOG_PREFIX]))
    
    if CONF_LOGGER_LOG_PREFIX in config:
        cg.add(var.set_logger_log_prefix(config[CONF_LOGGER_LOG_PREFIX]))
    
    # Register the text sensor for filter string updates if provided
    if CONF_FILTER_TEXT in config:
        text_obj = yield cg.get_variable(config[CONF_FILTER_TEXT])
        cg.add(var.register_filter_string_text(text_obj))
    
    # Parse filter string and add filters
    if config[CONF_FILTER_STRING]:
        # Set the filter string directly
        cg.add(var.set_filter_string(config[CONF_FILTER_STRING]))
    else:
        # Use the original filters list if filter_string is empty
        for filter_tag in config[CONF_FILTERS]:
            cg.add(var.add_filter(filter_tag))

# Register automation actions
@automation.register_action('syslog.log', SyslogLogAction, SYSLOG_LOG_ACTION_SCHEMA)
def syslog_log_action_to_code(config, action_id, template_arg, args):
    """Registers the syslog.log action for automations"""
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    
    template_ = yield cg.templatable(config[CONF_LEVEL], args, cg.uint8)
    cg.add(var.set_level(template_))
    template_ = yield cg.templatable(config[CONF_TAG], args, cg.std_string)
    cg.add(var.set_tag(template_))
    template_ = yield cg.templatable(config[CONF_PAYLOAD], args, cg.std_string)
    cg.add(var.set_payload(template_))
    
    yield var

@automation.register_action('syslog.add_filter', SyslogAddFilterAction, SYSLOG_ADD_FILTER_SCHEMA)
def syslog_add_filter_action_to_code(config, action_id, template_arg, args):
    """Registers the syslog.add_filter action for automations"""
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_TAG], args, cg.std_string)
    cg.add(var.set_tag(template_))
    yield var

@automation.register_action('syslog.remove_filter', SyslogRemoveFilterAction, SYSLOG_REMOVE_FILTER_SCHEMA)
def syslog_remove_filter_action_to_code(config, action_id, template_arg, args):
    """Registers the syslog.remove_filter action for automations"""
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_TAG], args, cg.std_string)
    cg.add(var.set_tag(template_))
    yield var

@automation.register_action('syslog.clear_filters', SyslogClearFiltersAction, SYSLOG_CLEAR_FILTERS_SCHEMA)
def syslog_clear_filters_action_to_code(config, action_id, template_arg, args):
    """Registers the syslog.clear_filters action for automations"""
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    yield var

@automation.register_action('syslog.set_filter_string', SyslogSetFilterStringAction, SYSLOG_SET_FILTER_STRING_SCHEMA)
def syslog_set_filter_string_action_to_code(config, action_id, template_arg, args):
    """Registers the syslog.set_filter_string action for automations"""
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_FILTER_STRING], args, cg.std_string)
    cg.add(var.set_filter_string(template_))
    yield var
