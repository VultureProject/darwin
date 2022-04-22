import logging
import socket
from os import kill, remove, access, rename, F_OK
from signal import SIGHUP
from time import sleep
from tools.filter import Filter, DEFAULT_LOG_FILE, DEFAULT_ALERTS_FILE
from tools.output import print_result
from core.utils import DEFAULT_PATH, FTEST_CONFIG, RESP_MON_STATUS_RUNNING
from tools.darwin_utils import count_file_lines
from core.utils import DEFAULT_PATH, FTEST_CONFIG, FTEST_CONFIG_NO_ALERT_LOG, RESP_MON_STATUS_RUNNING



def run():
    tests = [
        check_start_stop,
        check_pid_file,
        check_unix_socket_create_delete,
        check_unix_socket_connection,
        check_tcp_socket_create_delete,
        check_tcp_socket_connection,
        check_tcp6_socket_create_delete,
        check_tcp6_socket_connection,
        check_udp_socket_connection,
        check_udp6_socket_connection,
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
        check_write_logs,
        check_rotate_logs,
        check_rotate_logs_new_file_already_created,
        check_rotate_alerts,
        check_no_alerts_file_rotate_ok,
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


def check_unix_socket_create_delete():
    filter = Filter(filter_name="test", socket_type='unix')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    if not access(filter.socket, F_OK):
        logging.error("check_unix_socket_create_delete: Socket file not accesible")
        return False

    filter.stop()

    if access(filter.socket, F_OK):
        logging.error("check_unix_socket_create_delete: Socket file not deleted")
        return False

    return True


def check_unix_socket_connection():
    filter = Filter(filter_name="test", socket_type='unix')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        api = filter.get_darwin_api()
        api.call("test\n", filter_code=0x74657374, response_type="back")
        api.close()
    except Exception as e:
        logging.error("check_unix_socket_connection: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


def check_tcp_socket_create_delete():
    filter = Filter(filter_name="test", socket_type='tcp', socket_path='127.0.0.1:12323')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()
    with socket.socket(socket.AF_INET) as s:
        s.settimeout(2)
        res = s.connect_ex(('127.0.0.1',12323))
        s.close()

    if res != 0:
        logging.error("check_tcp_socket_create_delete: Socket file not accesible")
        return False
    filter.stop()

    with socket.socket(socket.AF_INET) as s:
        s.settimeout(2)
        res = s.connect_ex(('127.0.0.1',12323))
        s.close()
    
    if res == 0:
        logging.error("check_tcp_socket_create_delete: Socket file not deleted")
        return False

    return True


def check_tcp_socket_connection():
    filter = Filter(filter_name="test", socket_type='tcp', socket_path='127.0.0.1:12123')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        api = filter.get_darwin_api()
        api.call("test\n", filter_code=0x74657374, response_type="back")
        api.close()
    except Exception as e:
        logging.error("check_tcp_socket_connection: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


def check_tcp6_socket_create_delete():
    filter = Filter(filter_name="test", socket_type='tcp', socket_path='[::1]:12123')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    with socket.socket(socket.AF_INET6) as s:
        s.settimeout(2)
        res = s.connect_ex(('::1',12123))
        s.close()

    if res != 0:
        logging.error("check_tcp6_socket_create_delete: Socket file not accesible")
        return False

    filter.stop()

    with socket.socket(socket.AF_INET6) as s:
        s.settimeout(2)
        res = s.connect_ex(('::1',12123))
        s.close()
    
    if res == 0:
        logging.error("check_tcp6_socket_create_delete: Socket file not deleted")
        return False

    return True


def check_tcp6_socket_connection():
    filter = Filter(filter_name="test", socket_type='tcp', socket_path='[::1]:1111')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        api = filter.get_darwin_api()
        api.call("test\n", filter_code=0x74657374, response_type="back")
        api.close()
    except Exception as e:
        logging.error("check_tcp6_socket_connection: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


# These tests are not done as there is no reliable way to check if a udp socket is open and listening
# unreliable ways include listening for a icmp packet back but it depends on the 
# system and if the packet is blocked by firewalls

# def check_udp_socket_create_delete()
# def check_udp6_socket_create_delete()

def check_udp_socket_connection():
    filter = Filter(filter_name="test", socket_type='udp', socket_path='127.0.0.1:12123')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        api = filter.get_darwin_api()
        for i in range(5):
            api.call(f"udp test{i}", filter_code=0x74657374, response_type="no")
        # sleep to let the filter process the call
        sleep(2)
        # We check with 'check_line_in_filter_log' because udp can't return an answer, we must check alerts or logs
        for i in range(5):
            if not filter.check_line_in_filter_log(f"udp test{i}"):
                logging.error(f"No alert risen when it should have risen 'udp test{i}'")
                return False

        api.close()
    except Exception as e:
        logging.error("check_udp_socket_connection: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


def check_udp6_socket_connection():
    filter = Filter(filter_name="test", socket_type='udp', socket_path='[::1]:1111')

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    try:
        api = filter.get_darwin_api()
        api.call("test doppledidoo\n", filter_code=0x74657374, response_type="no")
        sleep(1) #let filter process
        # We check with 'check_line_in_filter_log' because udp can't return an answer, we must check alerts or logs
        if not filter.check_line_in_filter_log("test doppledidoo"):
            logging.error("No alert risen when it should have risen 'test doppledidoo'")
            return False
        api.close()
    except Exception as e:
        logging.error("check_udp6_socket_connection: Error connecting to socket: {}".format(e))
        return False

    filter.stop()
    return True


def check_socket_monitor_create_delete():
    filter = Filter(filter_name="test")

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

def check_write_logs():
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG)

    init_lines = count_file_lines(DEFAULT_LOG_FILE)

    filter.valgrind_start()
    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_write_logs: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        logging.error("check_write_logs: Process {} not stopping: {}".format(filter.process.pid, e))
        return False

    if init_lines == count_file_lines(DEFAULT_LOG_FILE):
        logging.error("check_write_logs: filter didn't write any logs in logfile")
        return False

    return True

def check_rotate_logs():
    error = ""
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG)

    filter.valgrind_start()

    # rename file to simulate log rotation
    rename(DEFAULT_LOG_FILE, DEFAULT_LOG_FILE + ".moved")
    # send rotate signal to filter
    kill(filter.process.pid, SIGHUP)

    lines_after_rotate = count_file_lines(DEFAULT_LOG_FILE + ".moved")

    # send a line to filter to trigger writting to logfile
    filter.send_single("test")

    if count_file_lines(DEFAULT_LOG_FILE + ".moved") > lines_after_rotate:
        error += "check_rotate_logs: new lines appended to old logfile"

    if count_file_lines(DEFAULT_LOG_FILE) == 0:
        error += "check_rotate_logs: no new lines written to new logfile"

    remove(DEFAULT_LOG_FILE + ".moved")

    if error:
        logging.error(error)
        return False

    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_rotate_logs: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        logging.error("check_rotate_logs: Process {} not stopping: {}".format(filter.process.pid, e))
        return False

    return True

def check_rotate_logs_new_file_already_created():
    """
    Same test as above, but test behaviour when file was already recreated after rotation
    (logrotate behaviour with 'create' option)
    """
    error = ""
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG)

    filter.valgrind_start()

    # rename file to simulate log rotation
    rename(DEFAULT_LOG_FILE, DEFAULT_LOG_FILE + ".moved")
    # create new empty file
    f = open(DEFAULT_LOG_FILE, 'w')
    f.close()
    # send rotate signal to filter
    kill(filter.process.pid, SIGHUP)

    lines_after_rotate = count_file_lines(DEFAULT_LOG_FILE + ".moved")

    # send a line to filter to trigger writting to logfile
    filter.send_single("test")

    if count_file_lines(DEFAULT_LOG_FILE + ".moved") > lines_after_rotate:
        error += "check_rotate_logs_new_file_already_created: new lines appended to old logfile"

    if count_file_lines(DEFAULT_LOG_FILE) == 0:
        error += "check_rotate_logs_new_file_already_created: no new lines written to new logfile"

    remove(DEFAULT_LOG_FILE + ".moved")

    if error:
        logging.error(error)
        return False

    try:
        # Test if filter is still running (did it crash ?)
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_rotate_logs_new_file_already_created: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        logging.error("check_rotate_logs_new_file_already_created: Process {} not stopping: {}".format(filter.process.pid, e))
        return False

    return True

def check_rotate_alerts():
    error = ""
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG)

    filter.valgrind_start()

    # rename file to simulate log rotation
    rename(DEFAULT_ALERTS_FILE, DEFAULT_ALERTS_FILE + ".moved")
    # send rotate signal to filter
    kill(filter.process.pid, SIGHUP)

    lines_after_rotate = count_file_lines(DEFAULT_ALERTS_FILE + ".moved")

    # send a line to filter to trigger writting to logfile
    filter.send_single("test")

    if count_file_lines(DEFAULT_ALERTS_FILE + ".moved") > lines_after_rotate:
        error += "check_rotate_alerts: new lines appended to old logfile"

    if count_file_lines(DEFAULT_ALERTS_FILE) == 0:
        error += "check_rotate_alerts: no new lines written to new logfile"

    remove(DEFAULT_ALERTS_FILE + ".moved")

    if error:
        logging.error(error)
        return False

    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_rotate_alerts: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        logging.error("check_rotate_alerts: Process {} not stopping: {}".format(filter.process.pid, e))
        return False

    return True

def check_no_alerts_file_rotate_ok():
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG_NO_ALERT_LOG)

    filter.valgrind_start()

    # send rotate signal to filter
    kill(filter.process.pid, SIGHUP)

    sleep(0.5)

    if not filter.check_run():
        logging.error("check_no_alerts_file_rotate_ok: Process {} crashed".format(filter.process.pid))
        return False

    if filter.stop() is not True:
        logging.error("check_no_alerts_file_rotate_ok: Process {} not stopping".format(filter.process.pid))
        return False

    return True
