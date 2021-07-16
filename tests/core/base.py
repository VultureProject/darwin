import logging
import socket
from os import kill, remove, access, F_OK
from time import sleep
from tools.filter import Filter
from tools.output import print_result
from core.utils import DEFAULT_PATH, FTEST_CONFIG, RESP_MON_STATUS_RUNNING




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
        api.call("udp test", filter_code=0x74657374, response_type="no")
        # sleep to let the filter process the call
        #TODO check alert
        api.call("udp test2", filter_code=0x74657374, response_type="no")
        api.call("udp test3", filter_code=0x74657374, response_type="no")
        api.call("udp test4", filter_code=0x74657374, response_type="no")
        api.call("udp test5", filter_code=0x74657374, response_type="no")
        sleep(2)

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
        api.call("test\n", filter_code=0x74657374, response_type="no")
        sleep(1) #let filter process
        # todo check alert
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
