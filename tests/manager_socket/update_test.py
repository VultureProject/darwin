import logging
from time import sleep
from manager_socket.utils import requests
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop


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

    darwin_configure("{}")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp != '{}':
        logging.error("no_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "update_filters", "filters": []}')
    if resp != '{"status": "OK"}':
        logging.error("no_filter_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "monitor"}')
    if resp != '{}':
        logging.error("no_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    return ret

def no_filter_to_one():
    conf = """{
  "logs_1": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }
}
"""

    log_conf = """{
  "log_file_path": "/tmp/logs_test.log"
}"""

    ret = True

    darwin_configure("{}")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp != '{}':
        logging.error("no_filter_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    resp = requests(b'{"type": "update_filters", "filters": ["logs_1"]}')
    if resp != '{"status": "OK"}':
        logging.error("no_filter_to_one: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "monitor"}')
    if resp != '{"logs_1": {}}':
        logging.error("no_filter_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
    return ret

def no_filter_to_many():
    conf = """{
  "logs_1": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    },
    "logs_2": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    },
    "logs_3": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }
}
"""

    log_conf = """{
  "log_file_path": "/tmp/logs_test.log"
}"""

    ret = True

    darwin_configure("{}")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp != '{}':
        logging.error("no_filter_to_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    resp = requests(b'{"type": "update_filters", "filters": ["logs_1", "logs_2", "logs_3"]}')
    if resp != '{"status": "OK"}':
        logging.error("no_filter_to_many: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "monitor"}')
    if resp != '{"logs_1": {}, "logs_2": {}, "logs_3": {}}':
        logging.error("no_filter_to_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
    return ret

def one_filter_to_none():
    conf = """{
  "logs_1": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }
}
"""

    log_conf = """{
  "log_file_path": "/tmp/logs_test.log"
}"""

    ret = True

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp != '{"logs_1": {}}':
        logging.error("one_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure("{}")    
    resp = requests(b'{"type": "update_filters", "filters": ["logs_1"]}')
    if resp != '{"status": "OK"}':
        logging.error("one_filter_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "monitor"}')
    if resp != '{}':
        logging.error("one_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
    return ret

def many_filters_to_none():
    conf = """{
  "logs_1": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    },
    "logs_2": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    },
    "logs_3": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }
}
"""

    log_conf = """{
  "log_file_path": "/tmp/logs_test.log"
}"""

    ret = True

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp != '{"logs_1": {}, "logs_2": {}, "logs_3": {}}':
        logging.error("many_filters_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure("{}")    
    resp = requests(b'{"type": "update_filters", "filters": ["logs_1", "logs_2", "logs_3"]}')
    if resp != '{"status": "OK"}':
        logging.error("many_filters_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "monitor"}')
    if resp != '{}':
        logging.error("many_filters_to_none: Mismatching second monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
    return ret

def many_filters_to_one():
    sleep(5)
    conf = """{
  "logs_1": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    },
    "logs_2": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    },
    "logs_3": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }
}
"""

    log_conf = """{
  "log_file_path": "/tmp/logs_test.log"
}"""

    ret = True

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp != '{"logs_1": {}, "logs_2": {}, "logs_3": {}}':
        logging.error("many_filters_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    conf = """{
  "logs_1": {
        "exec_path": "/home/darwin/filters/darwin_logs",
        "config_file":"/tmp/logs.conf",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }
}
"""

    darwin_configure(conf)    
    resp = requests(b'{"type": "update_filters", "filters": ["logs_1", "logs_2", "logs_3"]}')
    if resp != '{"status": "OK"}':
        logging.error("many_filters_to_one: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(b'{"type": "monitor"}')
    if resp != '{"logs_1": {}}':
        logging.error("many_filters_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
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
