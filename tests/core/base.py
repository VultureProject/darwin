import logging
import socket
from os import kill, remove, access, F_OK
from time import sleep
from tools.filter import Filter
from tools.output import print_result
from tools.logger import CustomAdapter
from core.utils import DEFAULT_PATH, FLOGS_CONFIG, RESP_MON_STATUS_RUNNING
from darwin import DarwinApi




def run():
    global logger
    glogger = logging.getLogger("BASE")

    tests = [
        check_start_stop,
        check_pid_file,
        check_socket_create_delete,
        check_socket_connection,
        check_socket_monitor_create_delete,
        check_socket_monitor_connection,
        check_start_wrong_conf,
        check_start_no_conf,
    ]

    for i in tests:
        logger = CustomAdapter(glogger, {'test_name': i.__name__})
        print_result("Basic tests: " + i.__name__, i)


def check_start_stop():
    filter = Filter(filter_name="logs")

    filter.configure(FLOGS_CONFIG)
    filter.valgrind_start()
    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logger.error("Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        return False
    
    return True


def check_pid_file():
    filter = Filter(filter_name="logs")
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.valgrind_start()

    try:
        with open(filter.pid) as f:
            pid = int(f.readline())
    except Exception as e:
        logger.error("Unable to read pid file: {}".format(e))
        return False

    try:
        kill(pid, 0)
    except OSError as e:
        logger.error("Process {} not running: {}".format(pid, e))
        return False

    filter.stop()

    if access(filter.pid, F_OK):
        logger.error("PID file not deleted")
        return False
    
    return True


def check_socket_create_delete():
    filter = Filter(filter_name="logs")
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.valgrind_start()

    if not access(filter.socket, F_OK):
        logger.error("Socket file not accesible")
        return False

    filter.stop()

    if access(filter.socket, F_OK):
        logger.error("Socket file not deleted")
        return False
    
    return True


def check_socket_connection():
    filter = Filter(filter_name="logs")
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.valgrind_start()

    try:
        api = DarwinApi(socket_path=filter.socket, socket_type="unix")
        api.call("test\n", filter_code="logs", response_type="back")
        api.close()
    except Exception as e:
        logger.error("Error connecting to socket: {}".format(e))
        return False

    filter.stop()    
    return True


def check_socket_monitor_create_delete():
    filter = Filter(filter_name="logs")
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.valgrind_start()

    if not access(filter.monitor, F_OK):
        logger.error("Socket file not accesible")
        return False

    filter.stop()

    if access(filter.monitor, F_OK):
        logger.error("Socket file not deleted")
        return False
    
    return True


def check_socket_monitor_connection():
    filter = Filter(filter_name="logs")
    pid = -1

    filter.configure(FLOGS_CONFIG)
    filter.valgrind_start()

    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(filter.monitor)
            data = s.recv(4096).decode()
            s.close()
        if RESP_MON_STATUS_RUNNING not in data:
            logger.error("Wrong response; got {}".format(data))
            return False
    except Exception as e:
        logger.error("Error connecting to socket: {}".format(e))
        return False

    filter.stop()    
    return True


def check_start_wrong_conf():
    filter = Filter(filter_name="logs")

    filter.configure("")
    filter.valgrind_start()
    sleep(2)
    
    if not access(filter.pid, F_OK):
        return True

    logger.error("Process running with wrong configuration")
    return False


def check_start_no_conf():
    filter = Filter(filter_name="logs")

    filter.valgrind_start()
    sleep(2)
    
    if not access(filter.pid, F_OK):
        return True

    logger.error("Process running with wrong configuration")
    return False