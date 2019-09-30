import sys
import os
import socket
import subprocess
from time import sleep
from conf import MANAGEMENT_SOCKET_PATH, DEFAULT_FILTER_PATH, FILTER_SOCKETS_DIR, FILTER_PIDS_DIR
from os import access, F_OK


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

    try:
        response = sock.recv(4096).decode()
    except Exception as e:
        print("Error: could not get the response: " + str(e), file=sys.stderr)
        return None

    return response

def check_pid_file(file):
    if access(file, F_OK):
        with open(file, 'r') as f:
            if f.read():
                return True
    print("Error: PID file check failed", file=sys.stderr)
    return False

def check_socket_file(socket_path):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    if access(socket_path, F_OK):
        try:
            sock.connect(socket_path)
        except socket.error as e:
            print(e, file=sys.stderr)
            return False
        sock.close()
        return True

    print("Error: could not access socket {}".format(socket_path), file=sys.stderr)
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

CONF_FLOGS_WRONG_CONF = """{
    "hello": "world"
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
RESP_STATUS_KO_GEN = '{"status": "KO"'
RESP_STATUS_KO_NO_PID = '{"status": "KO", "errors": [{"filter": "logs_1", "error": "PID file not accessible"}]}'
RESP_STATUS_KO_NO_RUN = '{"status": "KO", "errors": [{"filter": "logs_1", "error": "Process not running"}]}'
RESP_STATUS_KO_NO_MAIN_SOCK = '{"status": "KO", "errors": [{"filter": "logs_1", "error": "Main socket not created"}]}'
RESP_STATUS_KO_NO_MON_SOCK = '{"status": "KO", "errors": [{"filter": "logs_1", "error": "Monitoring socket not created"}]}'
RESP_STATUS_KO_NO_MON_SOCK_NREADY = '{"status": "KO", "errors": [{"filter": "logs_1", "error": "Monitoring socket not ready"}]}'
RESP_STATUS_KO_LIST = [ RESP_STATUS_KO_NO_PID,
                        RESP_STATUS_KO_NO_RUN,
                        RESP_STATUS_KO_NO_MAIN_SOCK,
                        RESP_STATUS_KO_NO_MON_SOCK,
                        RESP_STATUS_KO_NO_MON_SOCK_NREADY]
RESP_FILTER_NOT_EXISTING = '{"status": "KO", "errors": [{"filter": "tototititata", "error": "Filter not existing"}]}'