import logging
import threading
from time import sleep, time
from tools.redis_utils import RedisServer
from tools.output import print_result
from tools.filter import Filter

REDIS_SOCKET_PATH = "/tmp/redis.sock"
REDIS_LIST_TRIGGER = "trigger_redis_list"
REDIS_LIST_NAME = "redisTest"
REDIS_CHANNEL_TRIGGER = "trigger_redis_channel"
REDIS_CHANNEL_NAME = "redis.test"
FTEST_CONF_TEMPLATE = """{{
    "redis_socket_path": "{0}",
    "redis_list_name": "{1}",
    "redis_channel_name": "{2}"
}}""".format(REDIS_SOCKET_PATH, REDIS_LIST_NAME, REDIS_CHANNEL_NAME)

def run():
    tests = [
        simple_master_server,
        master_replica,
        master_replica_master_temp_fail,
        master_replica_transfer_no_healthcheck,
        master_replica_transfer_with_healthcheck,
        master_replica_failover_no_healthcheck,
        master_replica_failover_with_healthcheck,
        multi_thread_master,
        master_replica_discovery_rate_limiting
    ]

    for i in tests:
         print_result("Redis tests: " + i.__name__, i)



def simple_master_server():
    master = RedisServer(unix_socket=REDIS_SOCKET_PATH)

    filter = Filter()
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    try:
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
       logging.error("simple_master_server: Could not connect to test filter: {}".format(e))
       return False

    sleep(1)

    with master.connect() as redis_connection:
        num_list_entries = redis_connection.llen(REDIS_LIST_NAME)

    if num_list_entries != 1:
        logging.error("simple_master_server: wrong number of entries in the redis list {}: " +
                        "expected 1 but got {}".format(REDIS_LIST_NAME, str(num_list_entries)))
        return False

    return True



def master_replica():
    master = RedisServer(address="127.0.0.1", port=1234)
    replica = RedisServer(address="127.0.0.1", port=1235, unix_socket=REDIS_SOCKET_PATH, master=master)

    filter = Filter()
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    master.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.send_single(REDIS_CHANNEL_TRIGGER)
    except Exception as e:
       logging.error("master_replica: Could not connect to test filter: {}".format(e))
       return False

    sleep(1)

    message = master.channel_get_message()

    if message is '':
        logging.error("master_replica: expected to get a message in channel {} " +
                        "but got nothing".format(REDIS_CHANNEL_NAME))
        return False

    if message != REDIS_CHANNEL_TRIGGER:
        logging.error("master_replica: expected to get a message in channel {} saying '{}' " +
                        "but got '{}' instead".format(REDIS_CHANNEL_NAME, REDIS_CHANNEL_TRIGGER, message))
        return False

    return True



def master_replica_master_temp_fail():
    master = RedisServer(address="127.0.0.1", port=1234)
    replica = RedisServer(address="127.0.0.1", unix_socket=REDIS_SOCKET_PATH, master=master)

    filter = Filter()
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    try:
        # success
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("master_replica_master_temp_fail: Could not connect to test filter: {}".format(e))
        return False

    master.stop()
    sleep(1)

    try:
        # failure
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("master_replica_master_temp_fail: Could not connect to test filter: {}".format(e))
        return False

    sleep(1)

    if not filter.check_run():
        logging.error("master_replica_master_temp_fail: filter seems to have crashed when master Redis got offline")
        return False

    master.start()

    # rate limiting will prevent to reconnect immediately after failure, so there should have a wait
    # (rate limiting is not tested here)
    sleep(8)

    try:
        # success
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("master_replica_master_temp_fail: Could not connect to test filter: {}".format(e))
        return False

    with master.connect() as master_connection:
        num_list_entries = master_connection.llen(REDIS_LIST_NAME)

    if num_list_entries != 1:
        logging.error("master_replica_master_temp_fail: wrong number of entries in the redis list {}: " +
                        "expected 1 but got {}".format(REDIS_LIST_NAME, num_list_entries))
        return False

    return True



def master_replica_transfer(function_name, healthcheck):
    master = RedisServer(address="127.0.0.1", port=1234)
    replica = RedisServer(address="127.0.0.1", port=1235, unix_socket=REDIS_SOCKET_PATH, master=master)

    filter = Filter()
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    try:
        # success
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("{}: Could not connect to test filter: {}".format(function_name, e))
        return False

    # master becomes replica
    with master.connect() as master_connection:
        master_connection.slaveof(replica.address, replica.port)
    #replica becomes master
    with replica.connect() as replica_connection:
        replica_connection.slaveof()
    sleep(1)

    if healthcheck:
        sleep(8)

    try:
        # success
        return_code = filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("{}: Could not connect to test filter: {}".format(function_name, e))
        return False

    if return_code != 0:
        logging.error("{}: Filter didn't return correct code, " +
                        "waited for 0 but got {}".format(function_name, return_code))
        return False

    with replica.connect() as new_master_connection:
        num_entries = new_master_connection.llen(REDIS_LIST_NAME)

    if num_entries != 2:
        logging.error("{}: Wrong number of entries in {}, " +
                        "expected 2 but got {}".format(function_name, REDIS_LIST_NAME, num_entries))
        return False

    return True



def master_replica_transfer_no_healthcheck():
    return master_replica_transfer("master_replica_transfer_no_healthcheck", healthcheck=False)

def master_replica_transfer_with_healthcheck():
    return master_replica_transfer("master_replica_transfer_with_healthcheck", healthcheck=True)



def master_replica_failover(function_name, healthcheck):
    master = RedisServer(address="127.0.0.1", port=1234)
    replica = RedisServer(unix_socket=REDIS_SOCKET_PATH, master=master)

    filter = Filter()
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    try:
        # success
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("{}: Could not connect to test filter: {}".format(function_name, e))
        return False

    # master shuts down
    master.stop()
    #replica becomes master
    with replica.connect() as replica_connection:
        replica_connection.slaveof()
    sleep(1)

    if healthcheck:
        sleep(8)

    try:
        # success
        return_code = filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("{}: Could not connect to test filter: {}".format(function_name, e))
        return False

    if return_code != 0:
        logging.error("{}: Filter didn't return correct code, " +
                        "waited for 0 but got {}".format(function_name, return_code))
        return False

    with replica.connect() as new_master_connection:
        num_entries = new_master_connection.llen(REDIS_LIST_NAME)

    if num_entries != 2:
        logging.error("{}: Wrong number of entries in {}, " +
                        "expected 2 but got {}".format(function_name, REDIS_LIST_NAME, num_entries))
        return False

    return True



def master_replica_failover_no_healthcheck():
    return master_replica_failover(function_name="master_replica_failover_no_healthcheck", healthcheck=False)

def master_replica_failover_with_healthcheck():
    return master_replica_failover(function_name="master_replica_failover_with_healthcheck", healthcheck=True)



def multi_thread_master():
    master = RedisServer(unix_socket=REDIS_SOCKET_PATH)

    filter = Filter(nb_threads=5)
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    thread_list = []
    def thread_brute(filter, count_log):
        for count in range(0, count_log):
            try:
                filter.send_single(REDIS_LIST_TRIGGER)
            except:
                return False
        return True


    for num in range(0, 5):
        thread_list.append(threading.Thread(target=thread_brute, args=(filter, 500)))

    for thread in thread_list:
        thread.start()

    for thread in thread_list:
        thread.join()

    sleep(1)

    number = master.get_number_of_connections()

    # 5 threads
    if number != 5:
        logging.error("multi_thread_master: wrong number of active connections: expected 5 but got " + str(number))
        return False

    return True



def master_replica_discovery_rate_limiting():
    master = RedisServer(address="127.0.0.1", port=1234)
    replica = RedisServer(unix_socket=REDIS_SOCKET_PATH, master=master)

    filter = Filter(nb_threads=5)
    filter.configure(FTEST_CONF_TEMPLATE)
    filter.start()

    try:
        # success
        filter.send_single(REDIS_LIST_TRIGGER)
    except Exception as e:
        logging.error("master_replica_discovery_rate_limiting: Could not connect to test filter: {}".format(function_name, e))
        return False

    # master shuts down
    master.stop()

    #brutal
    def thread_brute_time(filter, time_end):
        end = time() + time_end
        while time() < end:
            # accelerate throughput by avoiding constant calls to time()
            for i in range(0,9):
                try:
                    filter.send_single(REDIS_LIST_TRIGGER)
                except:
                    return False
        return True

    thread_list = []

    for num in range(0, 5):
        thread_list.append(threading.Thread(target=thread_brute_time, args=(filter, 9)))

    # ought to crash if command fails
    initial_connections_num = replica.connect().info()['total_connections_received']

    for thread in thread_list:
        thread.start()

    for thread in thread_list:
        thread.join()

    # new generated connections generated, minus the one generated by the call to get it
    new_connections = replica.connect().info()['total_connections_received'] - initial_connections_num - 1

    if new_connections > 10:
        logging.error("master_replica_discovery_rate_limiting: Wrong number of new connection attempts, " +
                        "was supposed to have 10 new at most, but got ".format(new_connections))
        return False

    return True