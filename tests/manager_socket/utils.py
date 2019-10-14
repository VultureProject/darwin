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
        logging.error(msg)
        return None
    try:
        sock.sendall(bytes(request))
    except Exception as e:
        logging.error("manager_socket.utils.requests: Could not send the request: " + str(e))
        return None

    try:
        response = sock.recv(4096).decode()
    except Exception as e:
        logging.error("manager_socket.utils.requests: Could not get the response: " + str(e))
        return None

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
RESP_LOGS_1 = '"logs_1": {}'
RESP_LOGS_2 = '"logs_2": {}'
RESP_LOGS_3 = '"logs_3": {}'
RESP_STATUS_OK = '"status": "OK"'
RESP_STATUS_KO = '"status": "KO"'
RESP_ERROR_NO_PID = '"error": "PID file not accessible"'
RESP_ERROR_NO_RUN = '"error": "Process not running"'
RESP_ERROR_NO_MAIN_SOCK = '"error": "Main socket not created"'
RESP_ERROR_NO_MON_SOCK = '"error": "Monitoring socket not created"'
RESP_ERROR_NO_MON_SOCK_NOT_READY = '"error": "Monitoring socket not ready"'
RESP_ERROR_FILTER_NOT_EXISTING = '"error": "Filter not existing"'