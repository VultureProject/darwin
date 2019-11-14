import os
import redis
import logging
from time import sleep
from tools.redis_utils import RedisServer

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi, DarwinPacket

REDIS_SOCKET = "/tmp/redis_logs.sock"
REDIS_LIST = "test_ftanomaly"
LOG_FILE = "/tmp/test_ftanomaly.txt"

class TAnomaly(Filter):
    def __init__(self):
        super().__init__(filter_name="tanomaly")
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)

    def configure(self):
        content = '{{\n' \
                  '"redis_socket_path": "{redis_socket}",\n' \
                  '"redis_list_name": "{redis_list}",\n' \
                  '"log_file_path": "{log_file}"\n'\
                  '}}'.format(redis_socket=REDIS_SOCKET,
                              redis_list=REDIS_LIST,
                              log_file=LOG_FILE)
        super(TAnomaly, self).configure(content)

    def send(self, data):
        header = DarwinPacket(
            packet_type="other",
            response_type="no",
            filter_code=0x544D4C59,
            event_id=uuid.uuid4().hex,
            body_size=len(data)
        )

        api = DarwinApi(socket_type="unix", socket_path=self.socket)

        api.socket.sendall(header)
        api.socket.sendall(data)
        api.close()

    def get_data_from_redis(self):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.smembers(REDIS_LIST)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis: {}".format(e))
            return None

        return res




def run():
    tests = [
        well_formatted_data_test,
        data_too_short_ignored_test,
        not_data_string_ignored_test,
        not_data_list_ignored_test,
        invalid_ip_ignored_test,
        invalid_protocol_ignored_test,
        thread_working_test,
    ]

    for i in tests:
        print_result("tanomaly: " + i.__name__, i)

def redis_test(test_name, data, expected_data):

    def data_to_bytes(data):
        res = set()
        for d in data:
            res.add(str.encode(";".join(d)))

        return res

    ret = True

    # CONFIG
    tanomaly_filter = TAnomaly()
    tanomaly_filter.configure()

    # START FILTER
    if not tanomaly_filter.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=tanomaly_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        data,
        filter_code="TANOMALY",
        response_type="back",
    )

    redis_data = tanomaly_filter.get_data_from_redis()
    expected_data = data_to_bytes(expected_data)

    if redis_data!=expected_data:
        logging.error("{}: Expected this data : {} but got {} in redis".format(test_name, expected_data, redis_data))
        ret = False

    # CLEAN
    darwin_api.close()

    tanomaly_filter.clean_files()
    # ret = tanomaly_filter.valgrind_stop() or tanomaly_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not tanomaly_filter.valgrind_stop():
        ret = False

    return ret


def well_formatted_data_test():
    return redis_test(
        "well_formatted_data_test",
        [
            ["79.187.169.202","46.126.241.248","2677","1"],
            ["250.230.92.234","54.220.65.198","2922","6"],
            ["171.104.231.132","0.127.226.192","467","17"],
        ],
        [
            ["79.187.169.202","46.126.241.248","2677","1"],
            ["250.230.92.234","54.220.65.198","2922","6"],
            ["171.104.231.132","0.127.226.192","467","17"],
        ]
    )

def not_data_list_ignored_test():
    return redis_test(
        "not_list_data_ignored_test",
        [
            ["73.90.76.52","99.184.81.66","1017","17"],
            ["250.230.92.234","54.220.65.198","2922","6"],
            "250.230.92.234",
        ],
        [
            ["73.90.76.52","99.184.81.66","1017","17"],
            ["250.230.92.234","54.220.65.198","2922","6"],
        ]
    )

def not_data_string_ignored_test():
    return redis_test(
        "not_string_data_ignored_test",
        [
            ["73.90.76.52","99.184.81.66",1017,"17"],
            ["250.230.92.234","54.220.65.198","2922",6],
            ["143.92.16.229","142.228.238.233","705","17"],
        ],
        [
            ["143.92.16.229","142.228.238.233","705","17"],
        ]
    )

def data_too_short_ignored_test():
    return redis_test(
        "data_too_short_ignored_test",
        [
            ["73.90.76.52","99.184.81.66","1017","17"],
            ["250.230.92.234","54.220.65.198","2922","6"],
            ["143.92.16.229","142.228.238.233"],
        ],
        [
            ["73.90.76.52","99.184.81.66","1017","17"],
            ["250.230.92.234","54.220.65.198","2922","6"],
        ]
    )

def invalid_ip_ignored_test():
    return redis_test(
        "invalid_ip_ignored_test",
        [
            ["90.76.52","99.184.81.66","1017","17"],
            ["143.92.16.229","233","705","17"],
            ["250.230.92.234","54.220.65.198","2922","6"],
        ],
        [
            ["250.230.92.234","54.220.65.198","2922","6"],
        ]
    )

def invalid_protocol_ignored_test():
    return redis_test(
        "invalid_protocol_ignored_test",
        [
            ["64.183.225.150","111.166.166.114","526","0"],
            ["128.72.180.29","253.81.68.206","1195","40"],
            ["250.230.92.234","54.220.65.198","2922","6"],
        ],
        [
            ["250.230.92.234","54.220.65.198","2922","6"],
        ]
    )


def thread_working_test():

    ret = True

    # CONFIG
    tanomaly_filter = TAnomaly()
    tanomaly_filter.configure()

    # START FILTER
    if not tanomaly_filter.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=tanomaly_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            ["73.90.76.52","99.184.81.66","1017","17"],
            ["250.230.92.234","54.220.65.198","2922","6"],
            ["171.104.231.132","0.127.226.192","467","17"],
            ["42.214.30.108","246.163.54.146","2979","1"],
            ["79.187.169.202","46.126.241.248","2677","1"],
            ["57.126.101.247","255.171.17.199","2468","17"],
            ["102.27.128.38","75.125.227.149","2249","1"],
            ["116.145.214.73","182.20.121.254","1687","1"],
            ["248.78.140.91","112.67.123.34","1119","1"],
            ["47.159.155.135","117.9.1.88","1740","6"]
        ],
        filter_code="TANOMALY",
        response_type="back",
    )

    # We wait for the thread to activate
    sleep(302)

    redis_data = tanomaly_filter.get_data_from_redis()

    if redis_data != set() :
        logging.error("thread_working_test : Expected no data in Redis but got {}".format(redis_data))
        ret = False

    # CLEAN
    darwin_api.close()

    tanomaly_filter.clean_files()
    # ret = tanomaly_filter.valgrind_stop() or tanomaly_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not tanomaly_filter.valgrind_stop():
        ret = False

    return ret
