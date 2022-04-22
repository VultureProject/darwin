__author__ = "Theo BERTIN"
__credits__ = []
__license__ = "GPLv3"
__version__ = "1.2"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'Everything related to config parsing'

import logging
import json
from jsonschema import validators, Draft7Validator
import psutil


logger = logging.getLogger()

# Create a validator able to set defaults defined in schemas
def extend_with_default(validator_class):
    validate_properties = validator_class.VALIDATORS["properties"]

    def set_defaults(validator, properties, instance, schema):
        for property, subschema in properties.items():
            if "default" in subschema:
                instance.setdefault(property, subschema["default"])

        for error in validate_properties(
            validator, properties, instance, schema,
        ):
            yield error

    return validators.extend(
        validator_class, {"properties" : set_defaults},
    )

custom_validator = extend_with_default(Draft7Validator)


# helpful link for schema generation/comprehension/documentation:
# https://json-schema.org/understanding-json-schema/reference/
conf_v1_schema = {
    "$schema": "http://json-schema.org/schema#",
    "type": "object",
    "patternProperties": {
        ".+": {
            "type": "object",
            "properties": {
                "exec_path": {"type": "string"},
                "config_file": {"type": "string"},
                "nb_thread": {
                    "oneOf": [
                        {
                            "type": "integer",
                            "minimum": 1,
                            "maximum": 100,
                            "default": 5
                        },
                        {
                            "type": "string",
                            "pattern": "^[1]?[0-9]{0,2}$"
                        }
                    ]
                },
                "log_level": {
                    "type": "string",
                    "enum": ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "DEVELOPER"],
                    "default": "WARNING"
                    },
                "cache_size": {
                    "oneOf": [
                        {
                            "type": "integer",
                            "minimum": 0,
                            "default": 0
                        },
                        {
                            "type": "string",
                            "pattern": "^[0-9]+$"
                        }
                    ]
                },
                "output": {
                    "type": "string",
                    "enum": ["NONE", "RAW", "LOG", "PARSED"],
                    "default": "NONE"
                    },
                "next_filter": {
                    "type": "string",
                    "default": "no"
                    },
                "threshold": {
                    "type": "integer",
                    "default": 100
                    }
            },
            "required": ["exec_path", "config_file"],
            "additionalProperties": False
        }
    },
    "additionalProperties": False
}

conf_v2_schema = {
    "$schema": "http://json-schema.org/schema#",
    "type": "object",
    "properties": {
        "version": {
            "type": "number",
            "minimum": 2
            },
        "report_stats": {
            "type": "object",
            "properties": {
                "redis": {
                    "type": "object",
                    "properties": {
                        "unix_path": {"type": "string"},
                        "ip": {"type": "string"},
                        "port": {"type": "integer"},
                        "channel": {"type": "string"},
                        "list": {"type": "string"}
                    },
                    "anyOf": [
                        {"required": ["channel"]},
                        {"required": ["list"]}
                    ],
                    "dependencies": {
                        "port": ["ip"],
                        "ip": ["port"]
                    },
                    "additionalProperties": False
                },
                "file": {
                    "type": "object",
                    "properties": {
                        "filepath": {"type": "string"},
                        "permissions": {
                            "type": "number",
                            "minimum": 1,
                            "maximum": 777,
                            "default": 640
                        }
                    },
                    "required": ["filepath"],
                    "additionalProperties": False
                },
                "interval": {
                    "type": "integer",
                    "minimum": 1,
                    "default": 60
                },
                "proc_stats": {
                    "type": "array",
                    "items": {
                        "type": "string",
                        "enum": ['cmdline', 'connections', 'cpu_affinity', 'cpu_num', 'cpu_percent', 'cpu_times', 'create_time', 'cwd', 'environ', 'exe', 'gids', 'io_counters', 'ionice', 'memory_full_info', 'memory_info', 'memory_maps', 'memory_percent', 'name', 'nice', 'num_ctx_switches', 'num_fds', 'num_handles', 'num_threads', 'open_files', 'pid', 'ppid', 'status', 'terminal', 'threads', 'uids', 'username']
                    },
                    "default": ["cpu_percent", "memory_percent"],
                    "additionalProperties": False
                }
            },
            "additionalProperties": False
        },
        "filters": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "name": {"type": "string"},
                    "exec_path": {"type": "string"},
                    "config_file": {"type": "string"},
                    "nb_thread": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": 100,
                        "default": 5
                        },
                    "log_level": {
                        "type": "string",
                        "enum": ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "DEVELOPER"],
                        "default": "WARNING"
                        },
                    "cache_size": {
                        "type": "integer",
                        "minimum": 0,
                        "default": 0
                        },
                    "output": {
                        "type": "string",
                        "enum": ["NONE", "RAW", "LOG", "PARSED"],
                        "default": "NONE"
                        },
                    "next_filter": {
                        "OneOf":[
                            {"type": "string"},
                            {"type": "object",
                            "properties": {
                                "socket_type":{
                                    "type":"string",
                                    "enum": ["NONE", "UNIX", "TCP", "UDP"],
                                    "default":"NONE"
                                    },
                                "address_path": {"type": "string"}
                                }
                            }
                        ]
                    },
                    "threshold": {
                        "type": "integer",
                        "default": 100
                        },
                    "network": {
                        "type": "object",
                        "properties": {
                            "socket_type":{
                                "type":"string",
                                "enum": ["UNIX", "TCP", "UDP"],
                                "default":"UNIX"
                                },
                            "address_path": {"type": "string"}
                            }
                        }
                },
                "required": ["name", "exec_path", "config_file"],
                "additionalProperties": False
            }
        }
    },
    "additionalProperties": False,
    "required": ["version", "filters"]
}


#########################
# Configuration objects #
#########################
config_file = ""
filters = {}
stats_reporting = {}
#########################

class ConfParseError(Exception):
    """Raised when a configuration file couldn't be parsed"""
    pass


def load_conf(prefix, suffix, conf=""):
    global config_file
    global stats_reporting
    global filters
    logger.debug("Configurator: Trying to open config file...")

    if conf:
        config_file = conf

    try:
        with open(config_file, 'r') as f:
            logger.debug("Configurator: Loading config file")
            configuration = json.load(f)
    except Exception as e:
        logger.critical("Configurator: Unable to load configuration: {}".format(e))
        raise ConfParseError("Incorrect configuration format: {}".format(e))

    if configuration.get('version'):
        logger.debug("Configurator: found a 'version' field, trying v2 format...")
        try:
            custom_validator(conf_v2_schema).validate(configuration)
        except Exception as e:
            raise ConfParseError("Incorrect configuration format: {}".format(e))
        filters.clear()
        stats_reporting.clear()
        stats_reporting.update(configuration['report_stats'])
        for filter in configuration['filters']:
            filters[filter['name']] = filter
        logger.debug("Configurator: loaded v2 config successfully")
    else:
        logger.debug("Configurator: no 'version' field, trying v1 format...")
        try:
            custom_validator(conf_v1_schema).validate(configuration)
        except Exception as e:
            raise ConfParseError("Incorrect configuration format: {}".format(e))
        stats_reporting.clear()
        filters.clear()
        filters.update(configuration)
        logger.debug("Configurator: loaded v1 config successfully")

    complete_filters_conf(prefix, suffix)

def complete_filters_conf(prefix, suffix):
    for name, filter in filters.items():
        if name and not filter.get('name'):
            filter['name'] = name
        filter['status'] = psutil.STATUS_WAKING
        filter['failures'] = 0
        filter['extension'] = '.1'
        filter['pid_file'] = '{prefix}/run{suffix}/{filter}{extension}.pid'.format(prefix=prefix, suffix=suffix, filter=filter['name'], extension=filter['extension'])
                
        filter['socket'] = '{prefix}/sockets{suffix}/{filter}{extension}.sock'.format(prefix=prefix, suffix=suffix, filter=filter['name'], extension=filter['extension'])
        filter['socket_link'] = '{prefix}/sockets{suffix}/{filter}.sock'.format(prefix=prefix, suffix=suffix, filter=filter['name'])

        if 'network' not in filter:
            filter['network'] = { "socket_type":"UNIX", "address_path":filter['socket'] }
        elif filter['network']['socket_type'] == "UNIX":
            filter['network']['address_path'] = filter['socket']
        
        filter['monitoring'] = '{prefix}/sockets{suffix}/{filter}_mon{extension}.sock'.format(
            prefix=prefix, suffix=suffix,
            filter=filter['name'], extension=filter['extension']
        )
    
    # Next filter is setup with a second loop (we need network information already setup)
    for _, filter in filters.items():
        if 'next_filter' not in filter or filter['next_filter'] == '':
            filter['next_filter_network'] = { "socket_type":"NONE", "address_path":"no" }
        else:
            if isinstance(filter['next_filter'], str):
                #check other filters
                modified = False
                for _, other_filter in filters.items():
                    if filter['next_filter'] == other_filter['name']:
                        filter['next_filter_network'] = other_filter['network']
                        modified = True
                if not modified:
                    raise ConfParseError("Filter '{}' had next_filter configured to '{}' but it was not found in the configuration"
                                        .format(filter['name'], filter['next_filter']))
            else:
                filter['next_filter_network'] = filter['next_filter']
