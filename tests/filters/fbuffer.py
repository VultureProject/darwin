import os
import uuid
import json
import redis
import logging
from time import sleep, time

from tools.redis_utils import RedisServer

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi, DarwinPacket

REDIS_SOCKET = "/tmp/redis_buffer.sock"
ALERT_FILE = "/tmp/test_fbuffer.txt"
FILTER_CODE = 0x62756672

class Buffer(Filter):
    def __init__(self):
        super().__init__(filter_name="buffer", cache_size=1, path="../build/darwin_buffer")
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)
        self.test_data = None
        self.internal_redis = self.filter_name + "_bufferFilter_internal"

    def configure(self):
        content = '{{' \
                      '"log_file_path": "{log_file}",' \
                      '"redis_socket_path": "{redis_socket}",' \
                      '"input_format": [' \
                        '{{"name": "net_src_ip", "type": "string"}},' \
                        '{{"name": "net_dst_ip", "type": "string"}},' \
                        '{{"name": "net_dst_port", "type": "string"}},' \
                        '{{"name": "ip_proto", "type": "string"}},' \
                        '{{"name": "ip", "type": "string"}},' \
                        '{{"name": "hostname", "type": "string"}},' \
                        '{{"name": "os", "type": "string"}},' \
                        '{{"name": "proto", "type": "string"}},' \
                        '{{"name": "port", "type": "string"}},' \
                        '{{"name": "test_int", "type": "int"}}' \
                      '],' \
                      '"outputs": [' \
                        '{{' \
                          '"filter_type": "fbuffer",' \
                          '"filter_socket_path": "/tmp/buffer_2.sock",' \
                          '"interval": 10,' \
                          '"required_log_lines": 3,' \
                          '"redis_lists": [{{' \
                            '"source": "source_1",' \
                            '"name": "darwin_buffer_buffer"' \
                          '}},' \
                          '{{' \
                            '"source": "source_2",' \
                            '"name": "darwin_buffer_buffer_2"' \
                          '}}]' \
                        '}},' \
                        '{{' \
                          '"filter_type": "fanomaly",' \
                          '"filter_socket_path": "/tmp/anomaly.sock",' \
                          '"interval": 10,' \
                          '"required_log_lines": 5,' \
                          '"redis_lists": [{{' \
                            '"source": "source_1",' \
                            '"name": "darwin_buffer_anomaly"' \
                          '}},' \
                          '{{' \
                            '"source": "source_2",' \
                            '"name": "darwin_buffer_anomaly_2"' \
                          '}}]' \
                        '}}' \
                      ']' \
                    '}}'.format(log_file=ALERT_FILE,
                        redis_socket=REDIS_SOCKET)
        super().configure(content)

    def clean_files(self):
        super().clean_files()

        try:
            os.remove(ALERT_FILE)
        except:
            pass


    def send(self, data):
        header = DarwinPacket(
            packet_type="other",
            response_type="no",
            filter_code=FILTER_CODE,
            event_id=uuid.uuid4().hex,
            body_size=len(data)
        )

        api = DarwinApi(socket_type="unix", socket_path=self.socket)

        api.socket.sendall(header)
        api.socket.sendall(data)
        api.close()

    def get_internal_redis_data(self, redis_list_name):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.smembers(redis_list_name)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis list {redis_list_name}: {error}".format(error=e, redis_list_name=redis_list_name))
            return None

        return res

    def get_redis_alerts(self):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.lrange(REDIS_ALERT_LIST, 0, -1)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis: {}".format(e))
            return None

        return res

    def get_file_alerts(self):
        try:
            f = open(ALERT_FILE, 'r')
            return f.readlines()
        except OSError as e:
            logging.error("No alert file found: {}".format(e))
            return None

    def get_test_data(self):
        if self.test_data is not None:
            return self.test_data

        self.test_data = list()

        data_file = open(DATA_TEST, "r")
        for data in data_file:
            data = data[0:-1]
            self.test_data.append(data.split(";"))

        return self.test_data


def run():
    tests = [
        well_formatted_data_test,
        not_data_list_ignored_test,
        not_data_string_ignored_test,
        not_data_int_ignored_test,
        data_too_short_ignored_test,
        data_too_long_ignored_test,
        thread_working_test,
#        bad_configuration, # A décliner
#        next_filter_tests, # A décliner
#        fanomaly_connectors # A décliner
    ]

    for i in tests:
        print_result("buffer: " + i.__name__, i)


def redis_test(test_name, data, expected_data, redis_list_name):

    def data_to_bytes(data):
        res = set()
        for d in data:
            d = map(str, d)
            res.add(str.encode(';'.join(d)))

        return res

    ret = True

    # CONFIG
    buffer_filter = Buffer()
    buffer_filter.configure()

    # START FILTER
    if not buffer_filter.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=buffer_filter.socket,
                           socket_type="unix", )

    darwin_api.bulk_call(
        data,
        response_type="back",
    )

    redis_data = buffer_filter.get_internal_redis_data(redis_list_name)
    expected_data = data_to_bytes(expected_data)

    if redis_data!=expected_data:
        logging.error("{}: Expected this data : {} but got {} in redis".format(test_name, expected_data, redis_data))
        ret = False

    # CLEAN
    darwin_api.close()

    buffer_filter.clean_files()
    # ret = buffer_filter.valgrind_stop() or buffer_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not buffer_filter.valgrind_stop():
        ret = False

    return ret


def well_formatted_data_test():
    return redis_test(
        "well_formatted_data_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 2], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 1]  # Well formated
        ],
        [
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 2],
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 1]
        ],
        "darwin_buffer_buffer"
    )

def not_data_list_ignored_test():
    return redis_test(
        "not_list_data_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 3], # Well formated
            "source_1"
        ],
        [
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 3]
        ],
        "darwin_buffer_buffer"
    )

def not_data_string_ignored_test():
    return redis_test(
        "not_string_data_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 12], # Well formated
            ["source_1", 42, "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2"]
        ],
        [
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 12]
        ],
        "darwin_buffer_buffer"
    )

def not_data_int_ignored_test():
    return redis_test(
        "not_string_data_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 7], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", "8"]
        ],
        [
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 7]
        ],
        "darwin_buffer_buffer"
    )

def data_too_short_ignored_test():
    return redis_test(
        "data_too_short_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value"]
        ],
        [
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9]
        ],
        "darwin_buffer_buffer"
    )

def data_too_long_ignored_test():
    return redis_test(
        "data_too_short_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 10, 12]
        ],
        [
            ["net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9]
        ],
        "darwin_buffer_buffer"
    )

def thread_working_test():

    ret = True

    # CONFIG
    buffer_filter = Buffer()
    buffer_filter.configure()

    # START FILTER
    if not buffer_filter.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=buffer_filter.socket,
                           socket_type="unix", )

    darwin_api.bulk_call(
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 1], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 2], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_3", 3], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_4", 4], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_5", 5], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_6", 6], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_7", 7], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_8", 8], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 9]  # Well formated
        ],
        filter_code="DGA",
        response_type="back",
    )

    # We wait for the thread to activate
    sleep(15)

    redis_data = buffer_filter.get_internal_redis_data("source_1")

    if redis_data != set() :
        logging.error("thread_working_test : Expected no data in Redis but got {}".format(redis_data))
        ret = False

    # CLEAN
    darwin_api.close()

    buffer_filter.clean_files()
    # ret = buffer_filter.valgrind_stop() or buffer_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not buffer_filter.valgrind_stop():
        ret = False

    return ret

