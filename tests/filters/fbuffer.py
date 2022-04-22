import json
import os
import uuid
import redis
import logging
from time import sleep, time

from tools.redis_utils import RedisServer

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi, DarwinPacket
from filters.ftanomaly import DATA_TEST, EXPECTED_ALERTS

REDIS_SOCKET = "/tmp/redis.sock"
FILTER_CODE = 0x62756672
REDIS_ALERT_LIST = "darwin_buffer_test_alert"
REDIS_ALERT_CHANNEL = "darwin.buffer.test.alert"

class Buffer(Filter):
    def __init__(self):
        super().__init__(filter_name="buffer", cache_size=1)
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)
        self.test_data = None

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
                        '{{"name": "test_int", "type": "int"}},' \
                        '{{"name": "decimal", "type": "float"}}' \
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
                    '}}'.format(redis_socket=REDIS_SOCKET)
        super().configure(content)

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
        not_data_float_ignored_test,
        data_too_short_ignored_test,
        data_too_long_ignored_test,
        multiple_sources_test,
        multiple_outputs_redis_test,
        multiple_inputs_redis_test,
        missing_data_in_conf,
        thread_working_test,
        fanomaly_connector_and_send_test,
        fsofa_connector_test,
        sum_test_one_value,
        sum_test_multiple_values,
        sum_test_not_enough,
        sum_reset_existing_key,
        sum_existing_key_other_type
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
    darwin_api = buffer_filter.get_darwin_api()

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
        with buffer_filter.redis.connect() as redis_connection:
            redis_data = redis_connection.smembers(expect_pair[0])
        expected_data = data_to_bytes(expect_pair[1])

        if redis_data!=expected_data:
            logging.error("{}: Expected this data : {} but got {} in redis".format(test_name, expected_data, redis_data))
            ret = False

    # CLEAN
    darwin_api.close()

    # ret = buffer_filter.valgrind_stop() or buffer_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not buffer_filter.valgrind_stop():
        ret = False

    return ret

def well_formatted_data_test():
    return redis_test(
        "well_formatted_data_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 2, 0.3], # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 1, 4]  # Well formated
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
        "not_data_list_ignored_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 3, 4.2], # Well formated
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
        "not_data_string_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 12, -32.2], # Well formated
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
        "not_data_int_ignored_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 7, 1.2], # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", "8", -4.3]
        ],
        [(
            "darwin_buffer_anomaly",
            [
                ["net_src_ip_value_1", "1", "2", "3"]
            ]
        )]
    )

def not_data_float_ignored_test():
    return redis_test(
        "not_data_int_ignored_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 7, 1.2], # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 8, "6.66"]
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
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9, 13.4], # Well formated
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
        "data_too_long_ignored_test",
        [
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9, 12], # Well formated
            ["source_1", "net_src_ip_value__", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 10, 12, 13]
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
            ["source_2", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 9, 4.2], # Well formated
            ["source_3", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 8, 2.4]  # Well formated
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
        "multiple_outputs_redis_test",
        [
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value_1", "hostname_value", "os_value", "proto_value", "port_value_1", 9, 6.66], # Well formated
            ["source_2", "net_src_ip_value_2", "1", "2", "3", "ip_value_2", "hostname_value", "os_value", "proto_value", "port_value_2", 8, 6.66]  # Well formated
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
            ["source_1", "net_src_ip_value_1", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_1", 1, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_2", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_2", 2, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_3", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_3", 3, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_4", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_4", 4, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_5", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_5", 5, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_6", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_6", 6, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_7", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_7", 7, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_8", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_8", 8, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_9", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 9, 10.11],  # Well formated
            ["source_1", "net_src_ip_value_10", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 10, 13.14], # Well formated
            ["source_1", "net_src_ip_value_11", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 11, 13.14], # Well formated
            ["source_1", "net_src_ip_value_12", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 12, 13.14], # Well formated
            ["source_1", "net_src_ip_value_13", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 13, 13.14], # Well formated
            ["source_1", "net_src_ip_value_14", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 14, 13.14], # Well formated
            ["source_1", "net_src_ip_value_15", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 15, 13.14], # Well formated
            ["source_1", "net_src_ip_value_16", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 16, 13.14], # Well formated
            ["source_1", "net_src_ip_value_17", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 17, 13.14], # Well formated
            ["source_1", "net_src_ip_value_18", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 18, 13.14], # Well formated
            ["source_1", "net_src_ip_value_19", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 19, 13.14], # Well formated
            ["source_1", "net_src_ip_value_20", "1", "2", "3", "ip_value", "hostname_value", "os_value", "proto_value", "port_value_9", 20, 13.14]  # Well formated
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

    test_filter = Filter(filter_name="anomaly", socket_path="/tmp/anomaly.sock", socket_type='unix')
    test_filter.configure(config_test)


    # START FILTER
    if not buffer_filter.valgrind_start():
        return False

    if not test_filter.valgrind_start():
        print("Anomaly did not start")
        return False

    # SEND TEST
    darwin_api = buffer_filter.get_darwin_api()

    data = buffer_filter.get_test_data()

    darwin_api.bulk_call(
        data,
        response_type="back",
    )

    # We wait for the thread to activate
    sleep(15)

    with buffer_filter.redis.connect() as redis_connection:
        redis_data = redis_connection.smembers("darwin_buffer_anomaly")
    # redis_data = buffer_filter.get_internal_redis_set_data("darwin_buffer_anomaly")

    if redis_data != set() :
        logging.error("thread_working_test : Expected no data in Redis but got {}".format(redis_data))
        ret = False

    # CLEAN
    darwin_api.close()

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
                    '}}'.format(redis_socket=REDIS_SOCKET)

    )


def format_alert(alert, test_name):
        res = json.loads(alert)
        if "alert_time" in res:
            del res["alert_time"]
        else:
            logging.error("{} : No time in the alert : {}.".format(test_name, res))
        if "evt_id" in res:
            del res["evt_id"]
        return res

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

    test_filter = Filter(filter_name="anomaly", socket_path="/tmp/anomaly.sock", socket_type='unix')
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

    darwin_api = buffer_filter.get_darwin_api()

    darwin_api.bulk_call(
        data,
        response_type="back",
    )

    sleep(15)

    # GET REDIS DATA AND COMPARE

    with buffer_filter.redis.connect() as redis_connection:
        redis_data = redis_connection.lrange(REDIS_ALERT_LIST, 0, -1)
    expected_data = EXPECTED_ALERTS

    # remove evt_id from expected alerts (anomaly generates a real evt_id)
    for expected in expected_data:
        del expected['evt_id']

    if len(redis_data) != len(expected_data):
        logging.error("{}: Expecting a list of {} elements".format(test_name, len(expected_data)))
        ret = False


    redis_data = [format_alert(a.decode(), "fanomaly_connector_and_send_test") for a in redis_data]

    for data in redis_data:
        if not data in expected_data:
            logging.error("{}: entry wasn't expected {}".format(test_name, data))
            logging.error("{}: expected data in {}".format(test_name, expected_data))
            ret = False

    # CLEAN
    darwin_api.close()

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
            ["", "net_src_ip", "net_dst_ip", "port", "ip_proto", "ip_1", "hostname", "os", "proto", "port", 1, 1],
            ["", "net_src_ip", "net_dst_ip", "port", "ip_proto", "ip_2", "hostname", "os", "proto", "port", 2, 2],
            ["", "net_src_ip", "net_dst_ip", "port", "ip_proto", "ip_3", "hostname", "os", "proto", "port", 3, 3]
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


def sum_tests(test_name, values=[], required_log_lines=0, expected_alert=1, init_data=None, init_data_type="key", should_start=True):
    ret = True
    config_test =   '{{' \
                        '"redis_socket_path": "{redis_socket}",' \
                        '"alert_redis_list_name": "{redis_alert}",' \
                        '"alert_redis_channel_name": "{redis_alert_channel}"' \
                    '}}'.format(redis_socket=REDIS_SOCKET,
                                redis_alert=REDIS_ALERT_LIST,
                                redis_alert_channel=REDIS_ALERT_CHANNEL)

    config_buffer = '{{' \
                        '"redis_socket_path": "{redis_socket}",' \
                        '"input_format": [' \
                            '{{"name": "decimal", "type": "float"}}' \
                        '],' \
                        '"outputs": [' \
                            '{{' \
                                '"filter_type": "sum",' \
                                '"filter_socket_path": "/tmp/test.sock",' \
                                '"interval": 10,' \
                                '"required_log_lines": {required_log_lines},' \
                                '"redis_lists": [{{' \
                                    '"source": "",' \
                                    '"name": "darwin_buffer_test"' \
                                '}}]' \
                            '}}' \
                        ']' \
                    '}}'.format(redis_socket=REDIS_SOCKET,
                                required_log_lines=required_log_lines)

    # CONFIG
    buffer_filter = Buffer()
    buffer_filter.configure(config_buffer)

    test_filter = Filter(filter_name="test", socket_path="/tmp/test.sock", socket_type='unix')
    test_filter.configure(config_test)

    # Potentially add init data to redis
    if init_data:
        with buffer_filter.redis.connect() as redis_connection:
            if init_data_type == "key":
                redis_connection.set("darwin_buffer_test", init_data)
            elif init_data_type == "list":
                redis_connection.lpush("darwin_buffer_test", init_data)

    # START FILTER
    if should_start:
        if not test_filter.valgrind_start():
            print("test filter did not start")
            return False
        # FIXME cannot valgrind_start buffer while there is a problem with io_context destruction
        if not buffer_filter.valgrind_start():
            print("Buffer did not start")
            return False


        # SEND values
        darwin_api = buffer_filter.get_darwin_api()
        for test_value in values:
            darwin_api.call(
                ["", test_value],
                response_type="back",
            )

        sleep(15)

        # GET REDIS DATA AND COMPARE
        with buffer_filter.redis.connect() as redis_connection:
            redis_data = redis_connection.lrange(REDIS_ALERT_LIST, 0, -1)

        if len(redis_data) != expected_alert:
            logging.error("{}: Expecting {} alert, but got {}".format(test_name, "one" if expected_alert else "no", len(redis_data)))
            darwin_api.close()
            return  False

        if expected_alert:
            if not buffer_filter.validate_alert_line(redis_data[0]):
                logging.error("{}: could not validate alert\n{}".format(test_name, redis_data[0]))
                darwin_api.close()
                return  False

            # already validated as a json by validate_alert_line()
            log_line = json.loads(redis_data[0])
            parsed_entry = json.loads(log_line['entry'])

            if not len(parsed_entry) == 1:
                logging.error("{}: there should only be 2 fields in output 'entry', but got {}".format(test_name, len(parsed_entry)))
                logging.error("{}: entry is {}".format(test_name, parsed_entry))

            # Test the delta of real and theoretical values, as Redis rounds up floats a bit "coarsely"
            if abs(float(parsed_entry[0]) - sum(values)) > 1e-10:
                logging.error("{}: alert entry is not what was expected\nexpected {}\nbut got {}".format(test_name, sum(values), parsed_entry[0]))
                logging.error("{}: log is {}".format(test_name, log_line))
                ret = False

        # CLEAN
        darwin_api.close()

        test_filter.clean_files()
        # ret = buffer_filter.valgrind_stop() or buffer_filter.valgrind_stop()
        # would erase upper ret if this function return True
        # FIXME cannot valgrind_stop buffer while there is a problem with io_context destruction
        if not buffer_filter.valgrind_stop():
            ret = False

        if not test_filter.valgrind_stop():
            ret = False
    else:
        #filter should not start
        if buffer_filter.valgrind_start():
            logging.error("{}: buffer filter started but it shouldn't have".format(test_name))
            return False

    return ret


def sum_test_one_value():
    # Buffer filter should send [-123.4] to test filter
    return sum_tests("sum_test_one_value", values=[-123.4])

def sum_test_multiple_values():
    # Buffer filter should send [12263.6] (-123.4 + 42 + 12345) to test filter
    return sum_tests("sum_test_multiple_values", values=[-123.4, 42, 12345])

def sum_test_not_enough():
    # Buffer filter should not send data even though sum is not null, as required_log_lines > abs(round(value))
    return sum_tests("sum_test_multiple_values", values=[10.1], required_log_lines=11, expected_alert=0)

def sum_reset_existing_key():
    # Buffer should send 666.6, as initial 333.4 value in key should be deleted
    return sum_tests("sum_reset_existing_key", values=[666.6], required_log_lines=1, init_data=333.4)

def sum_existing_key_other_type():
    # Buffer shouldn't start as the key used for the sum is already present AND another type
    # (probably already used for someting else)
    return sum_tests("sum_existing_key_other_type", init_data=42, init_data_type="list", should_start=False)