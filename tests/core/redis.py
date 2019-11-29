import logging
import threading
from time import sleep
from tools.logger import CustomAdapter
from tools.redis_utils import RedisServer
from tools.output import print_result
from filters.flogs import Logs


REDIS_SOCKET_PATH = "/tmp/redis.socket"
REDIS_LIST_NAME = "redisTest"
FLOGS_CONF_TEMPLATE = """{{
    "redis_socket_path": "{0}",
    "redis_list_name": "{1}"
}}""".format(REDIS_SOCKET_PATH, REDIS_LIST_NAME)

def run():
    global logger
    glogger = logging.getLogger("REDIS")

    tests = [
        simple_master_server,
        master_slave,
        master_slave_master_fail,
        master_slave_master_off,
        master_timeout_restart,
        multi_thread_master
    ]

    for i in tests:
        logger = CustomAdapter(glogger, {'test_name': i.__name__})
        print_result("Redis tests: " + i.__name__, i)


def simple_master_server():
    master = RedisServer(unix_socket="/tmp/redis.socket")

    filter = Logs(redis_server=master, logger=logger)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'The cake is a lie')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    sleep(1)

    number = master.get_number_of_connections()

    if number != 2:
        logger.error("wrong number active connections: expected 2 but got " + str(number))
        return False

    return True


def master_slave():
    master = RedisServer(address="127.0.0.1", port=1234)
    slave = RedisServer(unix_socket="/tmp/redis.socket", master=master)

    filter = Logs(redis_server=slave, logger=logger)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'It s dangerous out there. Take this sword.')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    sleep(1)

    numberMaster = master.get_number_of_connections()
    numberSlave = slave.get_number_of_connections()

    # master : python client, filter connection, slave ping
    # slave : python client, filter connection, master ping
    if numberMaster != 3 and numberSlave != 2:
        logger.error("expected 2 master and 1 slave connections, but got " + str(numberMaster) + " and " + str(numberSlave))
        return False

    return True

def master_slave_master_fail():
    master = RedisServer(address="127.0.0.1", port=1234)
    slave = RedisServer(unix_socket="/tmp/redis.socket", master=master)

    filter = Logs(redis_server=slave, logger=logger)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'It s dangerous out there. Take this sword.')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    master.stop()
    sleep(1)

    try:
        filter.log(b'You re gonna have a bad time.')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    sleep(1)

    if not filter.check_run():
        logger.error("filter seems to have crashed when master Redis got offline")
        return False

    number = slave.get_number_of_connections()

    if number != 2:
        logger.error("filter is not connected to slave")
        return False

    return True


def master_slave_master_off():
    master = RedisServer(address="127.0.0.1", port=1234)
    slave = RedisServer(unix_socket="/tmp/redis.socket", master=master)

    filter = Logs(redis_server=slave, logger=logger)
    filter.configure(FLOGS_CONF_TEMPLATE)
    master.stop()

    filter.start()

    try:
        filter.log(b'It s dangerous out there. Take this sword.')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    if not filter.check_run():
        logger.error("filter seems to have crashed when master Redis got offline")
        return False

    sleep(1)

    number = slave.get_number_of_connections()

    if number != 2:
        logger.error("filter is not connected to slave")
        return False

    return True

def master_timeout_restart():
    master = RedisServer(unix_socket="/tmp/redis.socket")

    filter = Logs(redis_server=master, logger=logger)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'The cake is a lie')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    # Wait for Janitor to consider connection has timed out and disconnect
    sleep(25)

    number = master.get_number_of_connections()

    if number != 1:
        logger.error("wrong number active connections: expected 1 but got " + str(number))
        return False

    try:
        filter.log(b'The cake is a lie')
    except Exception:
        logger.error("Could not connect to logs filter")
        return False

    sleep(1)

    number = master.get_number_of_connections()

    if number != 2:
        logger.error("wrong number active connections: expected 2 but got " + str(number))
        return False

    return True

def multi_thread_master():
    master = RedisServer(unix_socket="/tmp/redis.socket")

    filter = Logs(redis_server=master, nb_threads=5, logger=logger)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    thread_list = []
    def thread_brute(filter, count_log):
        for count in range(0, count_log):
            try:
                filter.log(b'All work and no play makes Jake a dull boy.')
            except:
                return False
        return True


    for num in range(0, 5):
        thread_list.append(threading.Thread(target=thread_brute, args=(filter, 50)))

    for thread in thread_list:
        thread.start()

    for thread in thread_list:
        thread.join()

    sleep(1)

    number = master.get_number_of_connections()

    if number != 6:
        logger.error("master_timeout: wrong number active connections: expected 6 but got " + str(number))
        return False
    return True
