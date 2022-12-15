import logging
import socket
from os import kill, remove, access, rename, path, F_OK
from signal import SIGHUP
from time import sleep
from tools.filter import Filter, DEFAULT_LOG_FILE, DEFAULT_ALERTS_FILE
from tools.output import print_result
from tools.darwin_utils import count_file_lines
from core.utils import DEFAULT_PATH, FTEST_CONFIG, FTEST_CONFIG_NO_ALERT_LOG, RESP_MON_STATUS_RUNNING
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
        check_write_logs,
        check_rotate_logs,
        check_rotate_logs_new_file_already_created,
        check_rotate_alerts,
        check_no_alerts_file_rotate_ok,
        check_log_to_custom_file,
    ]

    for i in tests:
        print_result("Basic tests: " + i.__name__, i)


def check_start_stop():
    filter = Filter(filter_name="test")

    filter.configure(FTEST_CONFIG)
    filter.valgrind_start()

    if not filter.check_run():
        logging.error("check_start_stop: Process {} not running".format(filter.process.pid))
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

    # rename/move file to simulate log rotation
    rename(DEFAULT_LOG_FILE, DEFAULT_LOG_FILE + ".moved")
    # create new empty file
    f = open(DEFAULT_LOG_FILE, 'w')
    f.close()
    # send rotate signal to filter
    kill(filter.process.pid, SIGHUP)
    # Wait a bit for last lines to be written to current logfile
    sleep(0.5)

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
