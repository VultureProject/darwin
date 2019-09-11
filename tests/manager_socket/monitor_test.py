from manager_socket.utils import requests
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop


def run():
    tests = [
        multiple_filters_running,
        one_filters_running,
        no_filter,
    ]

    results = []

    for i in tests:
        results.append(("Monitor: " + i.__name__, i()))

    return results

def multiple_filters_running():
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

    ret = False

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp == '{"logs_1": {}, "logs_2": {}, "logs_3": {}}':
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
    return ret

def one_filters_running():
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

    ret = False

    darwin_configure(conf)
    darwin_configure(log_conf, path="/tmp/logs.conf")
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp == '{"logs_1": {}}':
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path="/tmp/logs.conf")
    return ret

def no_filter():
    conf = "{}"
    ret = False

    darwin_configure(conf)
    process = darwin_start()

    resp = requests(b'{"type": "monitor"}')
    if resp == '{}':
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    return ret
