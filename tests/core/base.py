import logging
import socket
from os import kill, remove, access, F_OK
from tools.filter import Filter
from tools.output import print_result
from core.utils import DEFAULT_PATH
from darwin import DarwinApi


FLOGS_CONFIG = '{"log_file_path": "/tmp/logs_test.log"}'

def run():
    tests = [
        check_start_stop,
        check_pid_file,
        check_socket_create_delete,
        check_socket_connection,
        check_socket_monitor_create_delete,
        check_socket_monitor_connection
    ]

    for i in tests:
        print_result("Basic tests: " + i.__name__, i())


def check_start_stop():
    filter = Filter(DEFAULT_PATH)

    filter.configure(FLOGS_CONFIG)
    filter.start()
    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_start_stop: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        return False
    
    return True


def check_pid_file():
    filter = Filter(DEFAULT_PATH)
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.start()

    try:
        with open(filter.pid) as f:
            pid = int(f.readline())
    except Exception as e:
        logging.error("check_pid: Unable to read pid file: {}".format(e))
        return False

    try:
        kill(pid, 0)
    except OSError as e:
        logging.error("check_pid: Process {} not running: {}".format(pid, e))
        return False

    filter.stop()

    if access(filter.pid, F_OK):
        logging.error("check_pid: PID file not deleted")
        return False
    
    return True


def check_socket_create_delete():
    filter = Filter(DEFAULT_PATH)
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.start()

    if not access(filter.socket, F_OK):
        logging.error("check_socket_create_delete: Socket file not accesible")
        return False

    filter.stop()

    if access(filter.socket, F_OK):
        logging.error("check_socket_create_delete: Socket file not deleted")
        return False
    
    return True


def check_socket_connection():
    filter = Filter(DEFAULT_PATH)
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.start()

    try:
        api = DarwinApi(socket_path=filter.socket, socket_type="unix")
        api.call("test\n", filter_code="logs", response_type="back")
        api.close()
    except Exception as e:
        logging.error("check_socket_connection_back: Error connecting to socket: {}".format(e))
        return False

    filter.stop()    
    return True


def check_socket_monitor_create_delete():
    filter = Filter(DEFAULT_PATH)
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.start()

    if not access(filter.monitor, F_OK):
        logging.error("check_socket_monitor_create_delete: Socket file not accesible")
        return False

    filter.stop()

    if access(filter.monitor, F_OK):
        logging.error("check_socket_monitor_create_delete: Socket file not deleted")
        return False
    
    return True


def check_socket_monitor_connection():
    filter = Filter(DEFAULT_PATH)
    pid = -1
    result = None

    filter.configure(FLOGS_CONFIG)
    filter.start()

    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(filter.monitor)
            data = s.recv(4096)
            s.close()
        if data != b'{}\x00':
            logging.error("check_socket_monitor_connection: Wrong response; got {}".format(data))
            return False
    except Exception as e:
        logging.error("check_socket_monitor_connection: Error connecting to socket: {}".format(e))
        return False

    if result != "test\n":
        logging.error("check_socket_monitor_connection: Result differs. Got {}".format(result))

    filter.stop()    
    return True
