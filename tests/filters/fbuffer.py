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
from filters.fanomaly import Anomaly

REDIS_SOCKET = "/var/sockets/redis/redis.sock"
ALERT_FILE = "/tmp/test_fbuffer.txt"
FILTER_CODE = 0x62756672
DATA_TEST = "filters/data/anomalyData.txt"
REDIS_ALERT_LIST = "darwin_buffer_test_alert"

class Buffer(Filter):
    def __init__(self):
        super().__init__(filter_name="buffer", cache_size=1)
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)
        self.test_data = None
        self.internal_redis = self.filter_name + "_bufferFilter_internal"

    def configure(self, config=None):
        content = config if config else '{{' \
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
                          '"filter_type": "fanomaly",' \
                          '"filter_socket_path": "/tmp/anomaly.sock",' \
                          '"interval": 10,' \
                          '"required_log_lines": 5,' \
                          '"redis_lists": [{{' \
                            '"source": "",' \
                            '"name": "darwin_buffer_anomaly"' \
                          '}},' \
                          '{{' \
                            '"source": "source_2",' \
                            '"name": "darwin_buffer_anomaly_2"' \
                          '}},' \
                          '{{' \
                            '"source": "source_3",' \
                            '"name": "darwin_buffer_anomaly_3"' \
                          '}}]' \
                        '}},' \
                        '{{' \
                          '"filter_type": "fsofa",' \
                          '"filter_socket_path": "/tmp/buffer_sofa.sock",' \
                          '"interval": 10,' \
                          '"required_log_lines": 3,' \
                          '"redis_lists": [{{' \
                            '"source": "",' \
                            '"name": "darwin_buffer_sofa"' \
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

    def get_internal_redis_set_data(self, redis_list_name):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.smembers(redis_list_name)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis list {redis_list_name}: {error}".format(error=e, redis_list_name=redis_list_name))
            return None

        return res

    def get_internal_redis_list_data(self, redis_list_name):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.lrange(redis_list_name, 0, -1)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis list {redis_list_name}: {error}".format(error=e, redis_list_name=redis_list_name))
            return None

        return res

    def get_test_data(self):
        if self.test_data is not None:
            return self.test_data

        self.test_data = list()

        data_file = open(DATA_TEST, "r")
        for data in data_file:
            data = ";" + data[0:-1] # Specific to Buffer as it needs an empty string or a source precised before each log line
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
        multiple_sources_test,
        multiple_outputs_redis_test,
        multiple_inputs_redis_test,
        missing_data_in_conf,
        thread_working_test,
        fanomaly_connector_and_send_test,
        fsofa_connector_test
    ]
    for i in tests:
            print_result("buffer: " + i.__name__, i)


def redis_test(test_name, data, expectations, config=None, bulk=True):

    def data_to_bytes(data):
        res = set()
        for d in data:
            d = map(str, d)
            res.add(str.encode(';'.join(d)))

        return res

    ret = True

    # CONFIG
    buffer_filter = Buffer()
    buffer_filter.configure(config)

    # START FILTER
    if not buffer_filter.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=buffer_filter.socket,
                           socket_type="unix", )

    if bulk:
        darwin_api.bulk_call(
            data,
            response_type="back",
        )
    else:
        for line in data:
            darwin_api.call(
                line,
                response_type="back"
            )

    for expect_pair in expectations:
        redis_data = buffer_filter.get_internal_redis_set_data(expect_pair[0])
        expected_data = data_to_bytes(expect_pair[1])

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
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 2], # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 1]  # Well formated
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"],
                ["net_src_ip_value_2", "1", "2", "3"]
            ]
        )]
    )

def not_data_list_ignored_test():
    return redis_test(
        "not_list_data_ignored_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 3], # Well formated
            "source_1"
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"]
            ]
        )]
    )

def not_data_string_ignored_test():
    return redis_test(
        "not_string_data_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 12], # Well formated
            ["source_1", 42, "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2"]
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value__", "1", "2", "3"]
            ]
        )]
    )

def not_data_int_ignored_test():
    return redis_test(
        "not_string_data_ignored_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 7], # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", "8"]
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"]
            ]
        )]
    )

def data_too_short_ignored_test():
    return redis_test(
        "data_too_short_ignored_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9], # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value"]
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"]
            ]
        )]
    )

def data_too_long_ignored_test():
    return redis_test(
        "data_too_short_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 10, 12]
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value__", "1", "2", "3"]
            ]
        )]
    )

def multiple_sources_test():
    return redis_test(
        "multiple_sources_test",
        [
            ["source_2", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9], # Well formated
            ["source_3", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 8]  # Well formated
        ],
        [(
            "darwin_buffer_anomaly_2",
            [
                ["net_src_ip_value_1", "1", "2", "3"]
            ]
        ),
        (
            "darwin_buffer_anomaly_3",
            [
                ["net_src_ip_value_2", "1", "2", "3"]
            ]
        ),
        (
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"],
                ["net_src_ip_value_2", "1", "2", "3"]
            ]
        )]
    )

def multiple_outputs_redis_test():
    return redis_test(
        "multiple_outputs_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value_1", "hostname_value", "os_value", "proto_value", "port_value_1", 9], # Well formated
            ["source_2", "net_src_ip_value_2", "1", "2", "3", "ip_value_2", "hostname_value", "os_value", "proto_value", "port_value_2", 8]  # Well formated
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"],
                ["net_src_ip_value_2", "1", "2", "3"]
            ]
        ),
        (
            "darwin_buffer_sofa",
            [
                ["ip_value_1", "hostname_value", "os_value", "proto_value", "port_value_1"],
                ["ip_value_2", "hostname_value", "os_value", "proto_value", "port_value_2"]
            ]
        )]
    )

def multiple_inputs_redis_test():
    return redis_test(
        "multiple_inputs_redis_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 1],  # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 2],  # Well formated
            ["source_1", "net_src_ip_value_3", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_3", 3],  # Well formated
            ["source_1", "net_src_ip_value_4", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_4", 4],  # Well formated
            ["source_1", "net_src_ip_value_5", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_5", 5],  # Well formated
            ["source_1", "net_src_ip_value_6", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_6", 6],  # Well formated
            ["source_1", "net_src_ip_value_7", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_7", 7],  # Well formated
            ["source_1", "net_src_ip_value_8", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_8", 8],  # Well formated
            ["source_1", "net_src_ip_value_9", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 9],  # Well formated
            ["source_1", "net_src_ip_value_10", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 10], # Well formated
            ["source_1", "net_src_ip_value_11", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 11], # Well formated
            ["source_1", "net_src_ip_value_12", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 12], # Well formated
            ["source_1", "net_src_ip_value_13", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 13], # Well formated
            ["source_1", "net_src_ip_value_14", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 14], # Well formated
            ["source_1", "net_src_ip_value_15", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 15], # Well formated
            ["source_1", "net_src_ip_value_16", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 16], # Well formated
            ["source_1", "net_src_ip_value_17", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 17], # Well formated
            ["source_1", "net_src_ip_value_18", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 18], # Well formated
            ["source_1", "net_src_ip_value_19", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 19], # Well formated
            ["source_1", "net_src_ip_value_20", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 20]  # Well formated
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"],
                ["net_src_ip_value_2", "1", "2", "3"],
                ["net_src_ip_value_3", "1", "2", "3"],
                ["net_src_ip_value_4", "1", "2", "3"],
                ["net_src_ip_value_5", "1", "2", "3"],
                ["net_src_ip_value_6", "1", "2", "3"],
                ["net_src_ip_value_7", "1", "2", "3"],
                ["net_src_ip_value_8", "1", "2", "3"],
                ["net_src_ip_value_9", "1", "2", "3"],
                ["net_src_ip_value_10", "1", "2", "3"],
                ["net_src_ip_value_11", "1", "2", "3"],
                ["net_src_ip_value_12", "1", "2", "3"],
                ["net_src_ip_value_13", "1", "2", "3"],
                ["net_src_ip_value_14", "1", "2", "3"],
                ["net_src_ip_value_15", "1", "2", "3"],
                ["net_src_ip_value_16", "1", "2", "3"],
                ["net_src_ip_value_17", "1", "2", "3"],
                ["net_src_ip_value_18", "1", "2", "3"],
                ["net_src_ip_value_19", "1", "2", "3"],
                ["net_src_ip_value_20", "1", "2", "3"],
            ]
        )],
        bulk=False
    )

def thread_working_test():
    ret = True

    config_buffer = '{{' \
                        '"redis_socket_path": "{redis_socket}",' \
                        '"input_format": [' \
                            '{{"name": "net_src_ip", "type": "string"}},' \
                            '{{"name": "net_dst_ip", "type": "string"}},' \
                            '{{"name": "net_dst_port", "type": "string"}},' \
                            '{{"name": "ip_proto", "type": "string"}}' \
                        '],' \
                        '"outputs": [' \
                            '{{' \
                                '"filter_type": "fanomaly",' \
                                '"filter_socket_path": "/tmp/anomaly.sock",' \
                                '"interval": 10,' \
                                '"required_log_lines": 5,' \
                                '"redis_lists": [{{' \
                                    '"source": "",' \
                                    '"name": "darwin_buffer_anomaly"' \
                                '}}]' \
                            '}}' \
                        ']' \
                    '}}'.format(redis_socket=REDIS_SOCKET)

    config_test =   '{{' \
                        '"redis_socket_path": "{redis_socket}",' \
                        '"alert_redis_list_name": "{redis_alert}",' \
                        '"log_file_path": "/var/log/darwin/alerts.log",' \
                        '"alert_redis_channel_name": "darwin.alerts"' \
                    '}}'.format(redis_socket=REDIS_SOCKET, redis_alert=REDIS_ALERT_LIST)


    # CONFIG
    buffer_filter = Buffer()
    buffer_filter.configure(config_buffer)

    test_filter = Filter(filter_name="anomaly", socket_path="/tmp/anomaly.sock")
    test_filter.configure(config_test)


    # START FILTER
    if not buffer_filter.valgrind_start():
        return False

    if not test_filter.start():
        print("Anomaly did not start")
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=buffer_filter.socket,
                           socket_type="unix", )

    data = buffer_filter.get_test_data()

    darwin_api.bulk_call(
        data,
        response_type="back",
    )

    # We wait for the thread to activate
    sleep(15)

    redis_data = buffer_filter.get_internal_redis_set_data("darwin_buffer_anomaly")

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

def missing_data_in_conf():
    return redis_test(
        "missing_data_in_conf",
        [
            ["source_1", "192.168.0.2", "195.25.10.44", "22", "17"], # Well formated, bad conf
            ["source_1", "192.168.0.2", "193.34.23.149", "12", "6"]  # Well formated, bad conf
        ],
        [(
            "darwin_buffer_anomaly",
            []
        )],
        config= '{{' \
                      '"redis_socket_path": "{redis_socket}",' \
                      '"input_format": [' \
                        '{{"name": "net_src_ip", "type": "string"}},' \
                        '{{"name": "net_dst_ip", "type": "string"}},' \
                        '{{"name": "net_dst_port", "type": "string"}},' \
                        '{{"name": "bad_argument_name", "type": "string"}}' \
                      '],' \
                      '"outputs": [' \
                        '{{' \
                          '"filter_type": "fanomaly",' \
                          '"filter_socket_path": "/tmp/buffer_anomaly.sock",' \
                          '"interval": 10,' \
                          '"required_log_lines": 5,' \
                          '"redis_lists": [{{' \
                            '"source": "source_1",' \
                            '"name": "darwin_buffer_anomaly"' \
                          '}}]' \
                        '}}' \
                      ']' \
                    '}}'.format(log_file=ALERT_FILE,
                        redis_socket=REDIS_SOCKET)

    )

def fanomaly_connector_and_send_test():
    test_name = "fanomaly_connector_and_send_test"
    ret = True
    config_test =   '{{' \
                        '"redis_socket_path": "{redis_socket}",' \
                        '"alert_redis_list_name": "{redis_alert}",' \
                        '"log_file_path": "/var/log/darwin/alerts.log",' \
                        '"alert_redis_channel_name": "darwin.alerts"' \
                    '}}'.format(redis_socket=REDIS_SOCKET, redis_alert=REDIS_ALERT_LIST)

    config_buffer = '{{' \
                        '"redis_socket_path": "{redis_socket}",' \
                        '"input_format": [' \
                            '{{"name": "net_src_ip", "type": "string"}},' \
                            '{{"name": "net_dst_ip", "type": "string"}},' \
                            '{{"name": "net_dst_port", "type": "string"}},' \
                            '{{"name": "ip_proto", "type": "string"}}' \
                        '],' \
                        '"outputs": [' \
                            '{{' \
                                '"filter_type": "fanomaly",' \
                                '"filter_socket_path": "/tmp/anomaly.sock",' \
                                '"interval": 10,' \
                                '"required_log_lines": 5,' \
                                '"redis_lists": [{{' \
                                    '"source": "",' \
                                    '"name": "darwin_buffer_test"' \
                                '}}]' \
                            '}}' \
                        ']' \
                    '}}'.format(redis_socket=REDIS_SOCKET)

    # CONFIG
    buffer_filter = Buffer()
    buffer_filter.configure(config_buffer)

    test_filter = Filter(filter_name="anomaly", socket_path="/tmp/anomaly.sock")
    test_filter.configure(config_test)

    # START FILTER
    if not buffer_filter.valgrind_start():
        print("Buffer did not start")
        return False

    if not test_filter.start():
        print("Anomaly did not start")
        return False

    # SEND TEST
    data = buffer_filter.get_test_data()

    darwin_api = DarwinApi(socket_path=buffer_filter.socket,
                           socket_type="unix")

    darwin_api.bulk_call(
        data,
        response_type="back",
    )

    sleep(15)

    # GET REDIS DATA AND COMPARE

    redis_data = buffer_filter.get_internal_redis_list_data(REDIS_ALERT_LIST)
    expected_data = '"details": {"ip": "192.168.110.2","udp_nb_host": 1.000000,"udp_nb_port": 252.000000,"tcp_nb_host": 0.000000,"tcp_nb_port": 0.000000,"icmp_nb_host": 0.000000,"distance": 246.193959}'

    if len(redis_data) != 1:
        logging.error("Expecting a single element list.")
        ret = False


    redis_data = [a.decode() for a in redis_data]


    if (expected_data not in redis_data[0]):
        logging.error("{}: Expected this data : {} but got {} in redis".format(test_name, expected_data, redis_data))
        ret = False

    # CLEAN
    darwin_api.close()

    buffer_filter.clean_files()
    test_filter.clean_files()
    # ret = buffer_filter.valgrind_stop() or buffer_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not buffer_filter.valgrind_stop():
        ret = False

    if not test_filter.stop():
        ret = False

    return ret

def fsofa_connector_test():
    return redis_test(
        "fsofa_connector_test",
        [
            ["", "net_src_ip", "net_dst_ip", "port", "ip_proto", "ip_1", "hostname", "os", "proto", "port", 1],
            ["", "net_src_ip", "net_dst_ip", "port", "ip_proto", "ip_2", "hostname", "os", "proto", "port", 2],
            ["", "net_src_ip", "net_dst_ip", "port", "ip_proto", "ip_3", "hostname", "os", "proto", "port", 3]
        ],
        [(
            "darwin_buffer_sofa",
            [
                ["ip_1", "hostname", "os", "proto", "port"],
                ["ip_2", "hostname", "os", "proto", "port"],
                ["ip_3", "hostname", "os", "proto", "port"]
            ]
        )]
    )