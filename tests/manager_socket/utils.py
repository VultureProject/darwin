import sys
import os
import socket
import subprocess
import logging
from time import sleep
from conf import MANAGEMENT_SOCKET_PATH, DEFAULT_FILTER_PATH, FILTER_SOCKETS_DIR, FILTER_PIDS_DIR
from os import access, F_OK


def requests(request):

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        sock.connect(MANAGEMENT_SOCKET_PATH)
    except socket.error as msg:
        logging.error("manager_socket.utils.requests: Could not connect to {}: {}".format(MANAGEMENT_SOCKET_PATH, msg))
        return ""
    try:
        sock.sendall(bytes(request))
    except Exception as e:
        logging.error("manager_socket.utils.requests: Could not send the request: " + str(e))
        return ""

    try:
        response = sock.recv(4096).decode()
    except Exception as e:
        logging.error("manager_socket.utils.requests: Could not get the response: " + str(e))
        return ""

    return response

def check_pid_file(file):
    try:
        with open(file, 'r') as f:
            if f.readline():
                return True
    except Exception as e:
        logging.error("manager_socket.utils.check_pid_file: PID file check failed: {}".format(e))
        return False

    logging.error("manager_socket.utils.check_pid_file: PID file check failed: File seems empty")
    return False

def check_socket_file(socket_path):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    if access(socket_path, F_OK):
        try:
            sock.connect(socket_path)
        except socket.error as e:
            logging.error(e)
            return False
        sock.close()
        return True

    logging.error("manager_socket.utils.check_socket_file: could not access socket {}".format(socket_path))
    return False

def check_filter_files(filter_name, extension=".1"):

    # Check PID file
    if not check_pid_file(FILTER_PIDS_DIR + filter_name + extension + ".pid"):
        return False

    # Check main socket file
    if not check_socket_file(FILTER_SOCKETS_DIR + filter_name + ".sock"):
        return False

    # Check real socket file
    if not check_socket_file(FILTER_SOCKETS_DIR + filter_name + extension + ".sock"):
        return False

    #Check monitoring socket
    if not check_socket_file(FILTER_SOCKETS_DIR + filter_name + "_mon" + extension + ".sock"):
        return False

    return True

# Configurations paths

PATH_CONF_FTEST = "/tmp/test.conf"

# Configurations

CONF_EMPTY = '{}'
CONF_ONE = """{{
  "test_1": {{
        "exec_path": "{0}darwin_test",
        "config_file":"{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FTEST)
CONF_ONE_V2 = """{{
    "version": 2,
    "filters": [
        {{
            "name": "test_1",
            "exec_path": "{0}darwin_test",
            "config_file": "{1}",
            "output": "NONE",
            "next_filter": "",
            "nb_thread": 1,
            "log_level": "DEBUG",
            "cache_size": 0
        }}
    ],
    "report_stats": {{
        "file": {{
            "filepath": "/tmp/darwin-stats",
            "permissions": 640
        }},
        "interval": 5
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FTEST)
CONF_THREE = """{{
  "test_1": {{
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    "test_2": {{
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    "test_3": {{
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FTEST)
CONF_THREE_V2 = """{{
    "version": 2,
    "filters": [
    {{
        "name": "test_1",
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    {{
        "name": "test_2",
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    {{
        "name": "test_3",
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
    ],
    "report_stats": {{
        "file": {{
            "filepath": "/tmp/darwin-stats",
            "permissions": 640
        }},
        "interval": 5
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FTEST)
CONF_THREE_ONE_WRONG = """{{
  "test_1": {{
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    "test_2": {{
        "exec_path": "wrong_path",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    "test_3": {{
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FTEST)
CONF_THREE_ONE_WRONG_V2 = """{{
    "version": 2,
    "filters": [
    {{
        "name": "test_1",
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    {{
        "name": "test_2",
        "exec_path": "wrong_path",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }},
    {{
        "name": "test_3",
        "exec_path": "{0}darwin_test",
        "config_file": "{1}",
        "output": "NONE",
        "next_filter": "",
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0
    }}
    ],
    "report_stats": {{
        "file": {{
            "filepath": "/tmp/darwin-stats",
            "permissions": 640
        }},
        "interval": 5
    }}
}}
""".format(DEFAULT_FILTER_PATH, PATH_CONF_FTEST)
CONF_FTEST = """{
    "log_file_path": "/tmp/test_test.log"
}"""

CONF_FTEST_WRONG_CONF = """{
    "fail_config": "yes"
}"""

# Requests

REQ_MONITOR      = b'{"type": "monitor"}'
REQ_MONITOR_CUSTOM_STATS = b'{"type": "monitor", "proc_stats": ["name", "pid", "memory_percent"]}'
REQ_MONITOR_ERROR = b'{"type": "monitor", "proc_stats": ["foo", "bar"]}'
REQ_UPDATE_EMPTY = b'{"type": "update_filters", "filters": []}'
REQ_UPDATE_ONE   = b'{"type": "update_filters", "filters": ["test_1"]}'
REQ_UPDATE_TWO   = b'{"type": "update_filters", "filters": ["test_2", "test_3"]}'
REQ_UPDATE_THREE = b'{"type": "update_filters", "filters": ["test_1", "test_2", "test_3"]}'
REQ_UPDATE_NON_EXISTING = b'{"type": "update_filters", "filters": ["tototititata"]}'
REQ_UPDATE_NO_FILTER = b'{"type": "update_filters"}'

# Responses

RESP_EMPTY     = '{}'

RESP_TEST_1 = '"test_1": {"status": "running", "connections": 0, "received": 0, "entryErrors": 0, "matches": 0, "failures": 0, "proc_stats": {'
RESP_TEST_2 = '"test_2": {"status": "running", "connections": 0, "received": 0, "entryErrors": 0, "matches": 0, "failures": 0, "proc_stats": {'
RESP_TEST_3 = '"test_3": {"status": "running", "connections": 0, "received": 0, "entryErrors": 0, "matches": 0, "failures": 0, "proc_stats": {'
RESP_STATUS_OK = '"status": "OK"'
RESP_STATUS_KO = '"status": "KO"'
RESP_ERROR_NO_PID = '"error": "PID file not accessible"'
RESP_ERROR_NO_RUN = '"error": "Process not running"'
RESP_ERROR_NO_MAIN_SOCK = '"error": "Main socket not created"'
RESP_ERROR_NO_MON_SOCK = '"error": "Monitoring socket not created"'
RESP_ERROR_NO_MON_SOCK_NOT_READY = '"error": "Monitoring socket not ready"'
RESP_ERROR_FILTER_NOT_EXISTING = '"error": "Filter not existing"'
