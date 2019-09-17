import sys
import os
import socket
import subprocess
from time import sleep
from conf import MANAGEMENT_SOCKET_PATH, DEFAULT_FILTER_PATH


def requests(request):

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        sock.connect(MANAGEMENT_SOCKET_PATH)
    except socket.error as msg:
        print(msg, file=sys.stderr)
        return None
    try:
        sock.sendall(bytes(request))
    except Exception as e:
        print("Error: Could not send the request: " + str(e), file=sys.stderr)
        return None

    response = sock.recv(4096).decode()

    return response


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
REQ_UPDATE_TWO   = b'{"type": "update_filters", "filters": ["logs_2", "logs_3"]}'
REQ_UPDATE_THREE = b'{"type": "update_filters", "filters": ["logs_1", "logs_2", "logs_3"]}'
REQ_UPDATE_NON_EXISTING = b'{"type": "update_filters", "filters": ["tototititata"]}'

# Responses

RESP_EMPTY     = '{}'
RESP_ONE       = '{"logs_1": {}}'
RESP_THREE     = '{"logs_1": {}, "logs_2": {}, "logs_3": {}}'
RESP_STATUS_OK = '{"status": "OK"}'
RESP_FILTER_NOT_EXISTING = '{"status": "KO", "errors": [{"filter": "tototititata", "error": "Filter not existing"}]}'