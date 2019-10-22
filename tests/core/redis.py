import logging
from time import sleep
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
    tests = [
        simple_master_server,
        master_slave,
        master_slave_master_fail,
        master_slave_master_off,
        master_timeout_restart
    ]

    for i in tests:
         print_result("Redis tests: " + i.__name__, i)


def simple_master_server():
    master = RedisServer(unix_socket="/tmp/redis.socket")

    filter = Logs(redis_server=master)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'The cake is a lie')
    except Exception:
        logging.error("simple_master_server: Could not connect to logs filter")
        return False

    sleep(1)

    number = master.get_number_of_connections()

    if number != 2:
        logging.error("simple_master_server: wrong number active connections: expected 2 but got " + str(number))
        return False

    return True


def master_slave():
    master = RedisServer(address="127.0.0.1", port=1234)
    slave = RedisServer(unix_socket="/tmp/redis.socket", master=master)

    filter = Logs(redis_server=slave)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'It s dangerous out there. Take this sword.')
    except Exception:
        logging.error("master_slave: Could not connect to logs filter")
        return False

    sleep(1)

    numberMaster = master.get_number_of_connections()
    numberSlave = slave.get_number_of_connections()

    # master : python client, filter connection, slave ping
    # slave : python client, filter connection, master ping
    if numberMaster != 3 and numberSlave != 2:
        logging.error("master_slave: expected 2 master and 1 slave connections, but got " + str(numberMaster) + " and " + str(numberSlave))
        return False

    return True

def master_slave_master_fail():
    master = RedisServer(address="127.0.0.1", port=1234)
    slave = RedisServer(unix_socket="/tmp/redis.socket", master=master)

    filter = Logs(redis_server=slave)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'It s dangerous out there. Take this sword.')
    except Exception:
        logging.error("master_slave_master_fail: Could not connect to logs filter")
        return False

    master.stop()
    sleep(1)

    try:
        filter.log(b'You re gonna have a bad time.')
    except Exception:
        logging.error("master_slave_master_fail: Could not connect to logs filter")
        return False

    sleep(1)

    if not filter.check_run():
        logging.error("master_slave_master_fail: filter seems to have crashed when master Redis got offline")
        return False

    number = slave.get_number_of_connections()

    if number != 2:
        logging.error("master_slave_master_fail: filter is not connected to slave")
        return False

    return True


def master_slave_master_off():
    master = RedisServer(address="127.0.0.1", port=1234)
    slave = RedisServer(unix_socket="/tmp/redis.socket", master=master)

    filter = Logs(redis_server=slave)
    filter.configure(FLOGS_CONF_TEMPLATE)
    master.stop()

    filter.start()

    try:
        filter.log(b'It s dangerous out there. Take this sword.')
    except Exception:
        logging.error("master_slave_master_off: Could not connect to logs filter")
        return False

    if not filter.check_run():
        logging.error("master_slave_master_off: filter seems to have crashed when master Redis got offline")
        return False

    sleep(1)

    number = slave.get_number_of_connections()

    if number != 2:
        logging.error("master_slave_master_off: filter is not connected to slave")
        return False

    return True

def master_timeout_restart():
    master = RedisServer(unix_socket="/tmp/redis.socket")

    filter = Logs(redis_server=master)
    filter.configure(FLOGS_CONF_TEMPLATE)
    filter.start()

    try:
        filter.log(b'The cake is a lie')
    except Exception:
        logging.error("master_timeout: Could not connect to logs filter")
        return False

    # Wait for Janitor to consider connection has timed out and disconnect
    sleep(25)

    number = master.get_number_of_connections()

    if number != 1:
        logging.error("master_timeout: wrong number active connections: expected 1 but got " + str(number))
        return False

    try:
        filter.log(b'The cake is a lie')
    except Exception:
        logging.error("master_timeout: Could not connect to logs filter")
        return False

    sleep(1)

    number = master.get_number_of_connections()

    if number != 2:
        logging.error("master_timeout: wrong number active connections: expected 2 but got " + str(number))
        return False

    return True