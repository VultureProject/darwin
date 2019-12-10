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
                    "next_filter": {"type": "string"},
                    "threshold": {
                        "type": "integer",
                        "default": 100
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


def load_conf(conf=""):
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
        raise e

    if configuration.get('version'):
        logger.debug("Configurator: found a 'version' field, trying v2 format...")
        try:
            custom_validator(conf_v2_schema).validate(configuration)
        except Exception as e:
            raise ConfParseError("Incorrect configuration format: {}".format(e.message))
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
            raise ConfParseError("Incorrect configuration format: {}".format(e.message))
        stats_reporting.clear()
        filters.clear()
        filters.update(configuration)
        logger.debug("Configurator: loaded v1 config successfully")

    complete_filters_conf()

def complete_filters_conf():
    for name, filter in filters.items():
        if name and not filter.get('name'):
            filter['name'] = name
        filter['status'] = psutil.STATUS_WAKING
        filter['failures'] = 0
        filter['extension'] = '.1'
        filter['pid_file'] = '/var/run/darwin/{filter}{extension}.pid'.format(filter=filter['name'], extension=filter['extension'])

        if not filter['next_filter']:
            filter['next_filter_unix_socket'] = 'no'
        else:
            filter['next_filter_unix_socket'] = '/var/sockets/darwin/{next_filter}.sock'.format(
                next_filter=filter['next_filter']
            )

        filter['socket'] = '/var/sockets/darwin/{filter}{extension}.sock'.format(filter=filter['name'], extension=filter['extension'])
        filter['socket_link'] = '/var/sockets/darwin/{filter}.sock'.format(filter=filter['name'])

        filter['monitoring'] = '/var/sockets/darwin/{filter}_mon{extension}.sock'.format(
            filter=filter['name'], extension=filter['extension']
        )
