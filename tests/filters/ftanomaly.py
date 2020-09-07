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

REDIS_SOCKET = "/tmp/redis_anomaly.sock"
REDIS_ALERT_LIST = "test_ftanomaly"
REDIS_ALERT_CHANNEL = "test.ftanomaly"
ALERT_FILE = "/tmp/test_ftanomaly.txt"
DATA_TEST = "filters/data/anomalyData.txt"

class TAnomaly(Filter):
    def __init__(self):
        super().__init__(filter_name="tanomaly", cache_size=1)
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)
        self.test_data = None
        self.internal_redis = self.filter_name + "_anomalyFilter_internal"

    def configure(self):
        content = '{{\n' \
            '"redis_socket_path": "{redis_socket}",\n' \
            '"alert_redis_list_name": "{redis_list}",\n' \
            '"alert_redis_channel_name": "{redis_channel}",\n' \
            '"log_file_path": "{log_file}",\n'\
            '"detection_frequency": 10\n'\
            '}}'.format(redis_socket=REDIS_SOCKET,
                        redis_list=REDIS_ALERT_LIST,
                        redis_channel=REDIS_ALERT_CHANNEL,
                        log_file=ALERT_FILE)
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
            filter_code=0x544D4C59,
            event_id=uuid.uuid4().hex,
            body_size=len(data)
        )

        api = DarwinApi(socket_type="unix", socket_path=self.socket)

        api.socket.sendall(header)
        api.socket.sendall(data)
        api.close()

    def get_internal_redis_data(self):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.smembers(self.internal_redis)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis: {}".format(e))
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
        data_too_short_ignored_test,
        not_data_string_ignored_test,
        not_data_list_ignored_test,
        invalid_ip_ignored_test,
        invalid_protocol_ignored_test,
        thread_working_test,
        alert_in_redis_test,
        alert_published_test,
        alert_in_file_test,
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

    darwin_api.bulk_call(
        data,
        filter_code="TANOMALY",
        response_type="back",
    )

    redis_data = tanomaly_filter.get_internal_redis_data()
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

    darwin_api.bulk_call(
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
    sleep(12)

    redis_data = tanomaly_filter.get_internal_redis_data()

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

def format_alert(alert, test_name):
        res = json.loads(alert)
        if "alert_time" in res :
            del res["alert_time"]
        else:
            logging.error("{} : No time in the alert : {}.".format(test_name, res))
        return res

def alert_in_redis_test():
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

    data = tanomaly_filter.get_test_data()
    darwin_api.bulk_call(
        data,
        filter_code="TANOMALY",
        response_type="back",
    )

    # We wait for the thread to activate
    sleep(12)

    # Too hard to test with "time" field, so it's removed,
    # but we check in alert received if this field is present
    expected_alerts = [
        {
            "alert_type": "darwin",
            "alert_subtype": "anomaly",
            "level": "high",
            "rule_name": "Abnormal Number of Unique Port Connexion",
            "tags": ["attack.discovery", "attack.t1046", "attack.command_and_control", "attack.defense_evasion", "attack.t1205"],
            "entry": "192.168.110.2",
            "score": 100,
            "evt_id": "-",
            "details": {
                "ip": "192.168.110.2",
                "udp_nb_host": 1.000000,
                "udp_nb_port": 252.000000,
                "tcp_nb_host": 0.000000,
                "tcp_nb_port": 0.000000,
                "icmp_nb_host": 0.000000,
                "distance": 246.193959
            }
        }
    ]

    redis_alerts = tanomaly_filter.get_redis_alerts()

    redis_alerts = [format_alert(a.decode(), "alerts_in_redis_test")
                    for a in redis_alerts]

    if len(redis_alerts)!=len(expected_alerts):
        ret = False
        logging.error("alerts_in_redis_test : Not the expected data in Redis. Got : {}, expected : {}".format(
            redis_alerts, expected_alerts))

    for a in redis_alerts:
        if a not in expected_alerts:
            ret = False
            logging.error("alerts_in_redis_test : Not the expected data in Redis. Got : {}, expected : {}".format(
                redis_alerts, expected_alerts))

    # CLEAN
    darwin_api.close()

    tanomaly_filter.clean_files()
    # ret = tanomaly_filter.valgrind_stop() or tanomaly_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not tanomaly_filter.valgrind_stop():
        ret = False

    return ret


def alert_published_test():
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

    darwin_api.bulk_call(
        tanomaly_filter.get_test_data(),
        filter_code="TANOMALY",
        response_type="back",
    )

    # Too hard to test with "time" field, so it's removed,
    # but we check in alert received if this field is present
    expected_alerts = [
        {
            "alert_type": "darwin",
            "alert_subtype": "anomaly",
            "level": "high",
            "rule_name": "Abnormal Number of Unique Port Connexion",
            "tags": ["attack.discovery", "attack.t1046", "attack.command_and_control", "attack.defense_evasion", "attack.t1205"],
            "entry": "192.168.110.2",
            "score": 100,
            "evt_id": "-",
            "details": {
                "ip": "192.168.110.2",
                "udp_nb_host": 1.000000,
                "udp_nb_port": 252.000000,
                "tcp_nb_host": 0.000000,
                "tcp_nb_port": 0.000000,
                "icmp_nb_host": 0.000000,
                "distance": 246.193959
            }
        }
    ]

    try:
        r = redis.Redis(
            unix_socket_path=tanomaly_filter.redis.unix_socket, db=0)
        pubsub = r.pubsub()
        pubsub.subscribe([REDIS_ALERT_CHANNEL])

        alert_received = 0
        timeout = 10  # in seconds
        timeout_start = time()
        # We stop waiting for the data after 10 seconds
        while (time() < timeout_start + timeout) and (alert_received < 2):
            message = pubsub.get_message()
            if message:
                if message["type"] == "message":
                    alert = format_alert(
                        message["data"].decode(), "alert_published_test")
                    if alert not in expected_alerts:
                        ret = False
                        logging.error(
                            "alert_published_test: Not the expected alert received in redis. Got {}".format(alert))
                    alert_received += 1
            sleep(0.001)

        if alert_received != len(expected_alerts):
            ret = False
            logging.error(
                "alert_published_test: Not the expected alerts number received on the channel")

        r.close()
    except Exception as e:
        ret = False
        logging.error(
            "alert_published_test: Error when trying to redis subscribe: {}".format(e))

    # CLEAN
    darwin_api.close()

    tanomaly_filter.clean_files()
    # ret = tanomaly_filter.valgrind_stop() or tanomaly_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not tanomaly_filter.valgrind_stop():
        ret = False

    return ret


def alert_in_file_test():
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

    darwin_api.bulk_call(
        tanomaly_filter.get_test_data(),
        filter_code="TANOMALY",
        response_type="back",
    )

    # We wait for the thread to activate
    sleep(12)

    # Too hard to test with "time" field, so it's removed,
    # but we check in alert received if this field is present
    expected_alerts = [
        {
            "alert_type": "darwin",
            "alert_subtype": "anomaly",
            "level": "high",
            "rule_name": "Abnormal Number of Unique Port Connexion",
            "tags": ["attack.discovery", "attack.t1046", "attack.command_and_control", "attack.defense_evasion", "attack.t1205"],
            "entry": "192.168.110.2",
            "score": 100,
            "evt_id": "-",
            "details": {
                "ip": "192.168.110.2",
                "udp_nb_host": 1.000000,
                "udp_nb_port": 252.000000,
                "tcp_nb_host": 0.000000,
                "tcp_nb_port": 0.000000,
                "icmp_nb_host": 0.000000,
                "distance": 246.193959
            }
        }
    ]

    redis_alerts = tanomaly_filter.get_file_alerts()

    redis_alerts = [format_alert(a, "alerts_in_file_test")
                    for a in redis_alerts]

    if redis_alerts is None:
        ret = False
        logging.error("alert_in_file_test : No alerts writing in alert file")

    if len(redis_alerts)!=len(expected_alerts):
        ret = False
        logging.error("alerts_in_file_test : Not the expected data in file. Got : {}, expected : {}".format(
            redis_alerts, expected_alerts))

    for a in redis_alerts:
        if a not in expected_alerts:
            ret = False
            logging.error("alerts_in_file_test : Not the expected data in file. Got : {}, expected : {}".format(
                redis_alerts, expected_alerts))

    # CLEAN
    darwin_api.close()

    tanomaly_filter.clean_files()
    # ret = tanomaly_filter.valgrind_stop() or tanomaly_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not tanomaly_filter.valgrind_stop():
        ret = False

    return ret