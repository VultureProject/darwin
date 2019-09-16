import logging
from time import sleep
from manager_socket.utils import requests
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop
from conf import DEFAULT_FILTER_PATH

# Configurations paths

PATH_CONF_FLOGS = "/tmp/logs.conf"

# Configurations

CONF_EMPTY = '{}'
CONF_ONE = """{{
  "logs_1": {{
        "exec_path": "{0}darwin_logs",
        "config_file":"{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FLOGS)
CONF_THREE = """{{
  "logs_1": {{
        "exec_path": "{0}darwin_logs",
        "config_file":"{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    "logs_2": {{
        "exec_path": "{0}darwin_logs",
        "config_file":"{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    "logs_3": {{
        "exec_path": "{0}darwin_logs",
        "config_file":"{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FLOGS)
CONF_FLOGS = """{
  "log_file_path": "/tmp/logs_test.log"
}"""

# Requests

REQ_MONITOR      = b'{"type": "monitor"}'
REQ_UPDATE_EMPTY = b'{"type": "update_filters", "filters": []}'
REQ_UPDATE_ONE   = b'{"type": "update_filters", "filters": ["logs_1"]}'
REQ_UPDATE_THREE = b'{"type": "update_filters", "filters": ["logs_1", "logs_2", "logs_3"]}'

# Responses

RESP_EMPTY     = '{}'
RESP_ONE       = '{"logs_1": {}}'
RESP_THREE     = '{"logs_1": {}, "logs_2": {}, "logs_3": {}}'
RESP_STATUS_OK = '{"status": "OK"}'


def run():
    tests = [
        no_filter_to_none,
        no_filter_to_one,
        no_filter_to_many,
        one_filter_to_none,
        # many_filters_to_none,
        # many_filters_to_one,
    ]

    results = []

    for i in tests:
        results.append(("Update: " + i.__name__, i()))

    return results


def no_filter_to_none():
    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_UPDATE_EMPTY)
    if resp != RESP_STATUS_OK:
        logging.error("no_filter_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    return ret


def no_filter_to_one():

    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_ONE)
    if resp != RESP_STATUS_OK:
        logging.error("no_filter_to_one: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("no_filter_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def no_filter_to_many():

    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_THREE)
    if resp != RESP_STATUS_OK:
        logging.error("no_filter_to_many: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("no_filter_to_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def one_filter_to_none():

    ret = True

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_EMPTY)    
    resp = requests(REQ_UPDATE_ONE)
    if resp != RESP_STATUS_OK:
        logging.error("one_filter_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("one_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_filters_to_none():

    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_filters_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_EMPTY)    
    resp = requests(REQ_UPDATE_THREE)
    if resp != RESP_STATUS_OK:
        logging.error("many_filters_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("many_filters_to_none: Mismatching second monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_filters_to_one():
 
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_filters_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_ONE)    
    resp = requests(REQ_UPDATE_THREE)
    if resp != RESP_STATUS_OK:
        logging.error("many_filters_to_one: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("many_filters_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def one_update_none():
    pass


def one_update_one():
    pass


def many_update_none():
    pass


def many_update_one():
    pass


def many_update_many():
    pass


def many_update_all():
    pass


def non_existing_filter():
    pass
