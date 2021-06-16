import logging
import socket
from os import kill, remove, access, path, F_OK
from time import sleep
from tools.filter import Filter
from tools.output import print_result
from core.utils import DEFAULT_PATH, FTEST_CONFIG, RESP_MON_STATUS_RUNNING
from darwin import DarwinApi




def run():
    tests = [
        check_start_stop,
        check_pid_file,
        check_socket_create_delete,
        check_socket_connection,
        check_socket_monitor_create_delete,
        check_socket_monitor_connection,
        check_start_wrong_conf,
        check_start_no_conf,
        check_start_invalid_thread_num,
        check_start_invalid_cache_num,
        check_start_invalid_threshold_num,
        check_start_outbound_thread_num,
        check_start_outbound_cache_num,
        check_start_outbound_threshold_num,
        check_log_to_custom_file,
    ]

    for i in tests:
        print_result("Basic tests: " + i.__name__, i)


def check_start_stop():
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_start_stop: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        return False

    return True


def check_pid_file():
    filter = Filter(filter_name="test")
    pid = -1

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

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
    filter = Filter(filter_name="test")
    pid = -1

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    if not access(filter.socket, F_OK):
        logging.error("check_socket_create_delete: Socket file not accesible")
        return False

    filter.stop()

    if access(filter.socket, F_OK):
        logging.error("check_socket_create_delete: Socket file not deleted")
        return False

    return True


def check_socket_connection():
    filter = Filter(filter_name="test")
    pid = -1

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        api = DarwinApi(socket_path=filter.socket, socket_type="unix")
        api.call("test\n", filter_code=0x74657374, response_type="back")
        api.close()
    except Exception as e:
        logging.error("check_socket_connection_back: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


def check_socket_monitor_create_delete():
    filter = Filter(filter_name="test")
    pid = -1

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    if not access(filter.monitor, F_OK):
        logging.error("check_socket_monitor_create_delete: Socket file not accesible")
        return False

    filter.stop()

    if access(filter.monitor, F_OK):
        logging.error("check_socket_monitor_create_delete: Socket file not deleted")
        return False

    return True


def check_socket_monitor_connection():
    filter = Filter(filter_name="test")
    pid = -1

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.connect(filter.monitor)
            data = s.recv(4096).decode()
            s.close()
        if RESP_MON_STATUS_RUNNING not in data:
            logging.error("check_socket_monitor_connection: Wrong response; got {}".format(data))
            return False
    except Exception as e:
        logging.error("check_socket_monitor_connection: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


def check_start_wrong_conf():
    filter = Filter(filter_name="test")

    filter.configure("")
    filter.valgrind_start()
    sleep(2)

    if not access(filter.pid, F_OK):
        return True

    logging.error("check_start_wrong_conf: Process running with wrong configuration")
    return False


def check_start_no_conf():
    filter = Filter(filter_name="test")

    filter.valgrind_start()
    sleep(2)

    if not access(filter.pid, F_OK):
        return True

    logging.error("check_start_wrong_conf: Process running with wrong configuration")
    return False

def check_start_invalid_thread_num():
    filter = Filter(filter_name="test", nb_threads="HelloThere")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    sleep(0.5)
    if filter.check_start():
        logging.error("check_start_invalid_thread_num: Process started when thread number was invalid")
        filter.stop()
        return False

    return True

def check_start_invalid_cache_num():
    filter = Filter(filter_name="test", cache_size="General")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    sleep(0.5)
    if filter.check_start():
        logging.error("check_start_invalid_cache_num: Process started when cache size was invalid")
        filter.stop()
        return False

    return True

def check_start_invalid_threshold_num():
    filter = Filter(filter_name="test", threshold="Kenobi")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    sleep(0.5)
    if filter.check_start():
        logging.error("check_start_invalid_threshold_num: Process started when threshold was invalid")
        filter.stop()
        return False

    return True


def check_start_outbound_thread_num():
    filter = Filter(filter_name="test", nb_threads="314159265358979323846264338327")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    sleep(0.5)
    if filter.check_start():
        logging.error("check_start_outbound_thread_num: Process started when thread number was out of bounds")
        filter.stop()
        return False

    return True

def check_start_outbound_cache_num():
    filter = Filter(filter_name="test", cache_size="950288419716939937510582097494")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    sleep(0.5)
    if filter.check_start():
        logging.error("check_start_outbound_cache_num: Process started when cache size was out of bounds")
        filter.stop()
        return False

    return True

def check_start_outbound_threshold_num():
    filter = Filter(filter_name="test", threshold="459230781640628620899862803482")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    sleep(0.5)
    if filter.check_start():
        logging.error("check_start_outbound_threshold_num: Process started when threshold was out of bounds")
        filter.stop()
        return False

    return True

def check_log_to_custom_file():
    outfilename = "/tmp/test_log_file.log"
    filter = Filter(filter_name="test", log_filepath=outfilename)

    if path.exists(outfilename):
        try:
            remove(outfilename)
        except:
            logging.error("check_log_to_custom_file: cannot remove file {} before launching filter".format(outfilename))
            return False

    filter.configure(FTEST_CONFIG)

    if not filter.valgrind_start():
        logging.error("check_log_to_custom_file: Process did not start correctly")
        return False

    try:
        with open(outfilename, 'r') as outfile:
            if outfile.buffer.peek() == b'':
                logging.error("check_log_to_custom_file: no log line in file {}".format(outfilename))
                return False
    except:
        logging.error("check_log_to_custom_file: logfile {} does not exist".format(outfilename))
        return False

    return True
