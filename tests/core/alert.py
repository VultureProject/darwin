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

REDIS_SOCKET = "/tmp/redis_alert.sock"
REDIS_ALERT_LIST = "test_alert"
REDIS_ALERT_CHANNEL = "test.alert"
ALERT_FILE = "/tmp/test_alert.txt"

class TestFilter(Filter):
    def __init__(self):
        super().__init__(filter_name='test')
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)

    def clean_files(self):
        super().clean_files()
        try:
            os.remove(ALERT_FILE)
        except:
            pass

    def send(self, data):
        api = DarwinApi(socket_type="unix", socket_path=self.socket)
        api.call([data], response_type='no')

    def get_redis_alerts(self):
        res = None

        try:
            r = redis.Redis(unix_socket_path=self.redis.unix_socket, db=0)
            res = r.lrange(REDIS_ALERT_LIST, 0, -1)
            r.close()
            r.delete(REDIS_ALERT_LIST)
        except Exception as e:
            logging.error("Unable to connect to redis: {}".format(e))
            return None

        return res

    @staticmethod
    def get_file_alerts():
        try:
            with open(ALERT_FILE, 'r') as f:
                return f.readlines()
        except OSError as e:
            return None


# Exhaustive Test List
#
# no file
# empty file
# empty config json
#
# file
# socket
# list
# channel
#
# wfile
# wsocket
# wlist
# wchannel
#
# list & channel
# file & socket
# file & list
# file & channel
# file & list & channel
# socket & list
# socket & channel
# socket & list & channel
# file & socket & list
# file & socket & channel
# file & socket & list & channel
#
# wfile & socket
# file & wsocket
# wfile & wsocket
#
# wfile & list
# file & wlist
# wfile & wlist
#
# wfile & channel
# file & wchannel
# wfile & wchannel
#
# wfile & list & channel
# file & wlist & channel
# file & list & wchannel
# wfile & wlist & channel
# file & wlist & wchannel
# wfile & list & wchannel
# wfile & wlist & wchannel
#
# wsocket & list
# socket & wlist
# wsocket & wlist
#
# wsocket & channel
# socket & wchannel
# wsocket & wchannel
#
# wsocket & list & channel
# socket & wlist & channel
# socket & list & wchannel
# wsocket & wlist & channel
# socket & wlist & wchannel
# wsocket & list & wchannel
# wsocket & wlist & wchannel
#
# wfile & socket & list
# file & wsocket & list
# file & socket & wlist
# wfile & wsocket & list
# file & wsocket & wlist
# wfile & socket & wlist
# wfile & wsocket & wlist
#
# wfile & socket & channel
# file & wsocket & channel
# file & socket & wchannel
# wfile & wsocket & channel
# file & wsocket & wchannel
# wfile & socket & wchannel
# wfile & wsocket & wchannel
#
# wfile & socket & list & channel
# wfile & wsocket & list & channel
# wfile & socket & wlist & channel
# wfile & socket & list & wchannel
# wfile & wsocket & wlist & channel
# wfile & socket & wlist & wchannel
# wfile & wsocket & list & wchannel
# wfile & wsocket & wlist & wchannel
def run():
    tests = [

    ]

    for i in tests:
        print_result("AlerManager: " + i.__name__, i)


def test(test_name: str, conf: str, log: str, expected_file=True, expected_list=True, expected_channel=True) -> bool:
    def check_file_output(filter: TestFilter, log: str, expected=True) -> bool:
        file_content = filter.get_file_alerts()
        if not file_content and not expected:
            return True
        elif not file_content and expected:
            if file_content is None:
                logging.error("Was expecting alert in log file, file is empty")
            else:
                logging.error("Was expecting alert in log file, file doesn't exist")
            return False
        elif file_content and not expected:
            logging.error("Expected log file to be empty or non extisting but got content")
            return False
        elif file_content and expected:
            if log + '\n' in file_content:
                return True
            logging.error("Log not in log file")
            return False


    def check_list_output(filter: TestFilter, log: str, expected=True) -> bool:
        alerts = filter.get_redis_alerts()
        if not alerts and not expected:
            return True
        elif not alerts and expected:
            logging.error("Was expecting alerts in the Redis list but got none")
            return False
        elif alerts and not expected:
            logging.error("Was not expecting alerts in the Redis list but got some")
            return False
        elif alerts and expected:
            if log in alerts:
                return True
            logging.error("The given lert could not be found in the Redist alert list")
            return False


    def check_channel_output(pubsub, log, expected=True) -> bool:
        message = pubsub.get_message(1)
        if message and expected:
            if message["type"] == "message":
                alert = message["data"].decode()
                if alert != log or alert != log.encode():
                    logging.error("Not the expected alert received in redis. Got {}".format(alert))
                    return False
                return True
            logging.error("Redis message received from channel has the wrong type")
            return False
        elif message and not expected:
            logging.error("Was expecting no message on Redis channel but got one")
            return False
        elif not message and expected:
            logging.error("Was expecting message on Redis channel but got none")
            return False
        elif not message and not expected:
            return True


    filter = TestFilter()
    if conf is not None:
        filter.configure(conf)

    try:
        r = redis.Redis(unix_socket_path=filter.redis.unix_socket, db=0)
        pubsub = r.pubsub()
        pubsub.subscribe([REDIS_ALERT_CHANNEL])

        filter.send(log)
        sleep(0.1)

        if not check_file_output(log, expected=expected_file):
            logging.error(test_name + ": File output check failed")
            return False
        if not check_list_output(log, expected=expected_list):
            logging.error(test_name + ": Redis list output check failed")
            return False
        if not check_channel_output(pubsub, log, expected=expected_list):
            logging.error(test_name + ": Redis channel output check failed")
            return False

        return True

    except Exception as e:
        logging.error("Got an unexpected error: " + str(e))
        return False

