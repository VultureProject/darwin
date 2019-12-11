import os
import uuid
import json
import redis
import logging
import functools
from time import sleep, time
from signal import SIGHUP

from tools.redis_utils import RedisServer

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

REDIS_SOCKET = "/tmp/redis_alert.sock"
REDIS_ALERT_LIST = "test_alert"
REDIS_ALERT_CHANNEL = "test.alert"
ALERT_FILE = "/tmp/test_alert.txt"
EMPTY_REDIS_SOCKET = ""
EMPTY_REDIS_ALERT_LIST = ""
EMPTY_REDIS_ALERT_CHANNEL = ""
EMPTY_ALERT_FILE = ""
WRONG_REDIS_SOCKET = 42
WRONG_REDIS_ALERT_LIST = 42
WRONG_REDIS_ALERT_CHANNEL = 42
WRONG_ALERT_FILE = 42

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
        try:
            os.remove(ALERT_FILE + '.1')
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
        {"test_name": "config_file_empty_json", "conf": {}, "log": "this is log 3", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file", "conf": {"log_file_path": ALERT_FILE}, "log": "this is log 4", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "socket", "conf": {"redis_socket_path": REDIS_SOCKET}, "log": "this is log 5", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "list", "conf": {"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 6", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "channel", "conf": {"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 7", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "empty_file", "conf": {"log_file_path": EMPTY_ALERT_FILE}, "log": "this is log 8", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "empty_socket", "conf": {"redis_socket_path": EMPTY_REDIS_SOCKET}, "log": "this is log 9", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "empty_list", "conf": {"alert_redis_list_name": EMPTY_REDIS_ALERT_LIST}, "log": "this is log 10", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "empty_channel", "conf": {"alert_redis_channel_name": EMPTY_REDIS_ALERT_CHANNEL}, "log": "this is log 11", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file", "conf": {"log_file_path": WRONG_ALERT_FILE}, "log": "this is log 12", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET}, "log": "this is log 13", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_list", "conf": {"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 14", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_channel", "conf": {"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 15", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "list__channel", "conf": {"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 16", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__socket", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": REDIS_SOCKET}, "log": "this is log 17", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "file__list", "conf": {"log_file_path": ALERT_FILE,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 18", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "file__channel", "conf": {"log_file_path": ALERT_FILE,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 19", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "file__list__channel", "conf": {"log_file_path": ALERT_FILE,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 20", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "socket__list", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 21", "expected_file": False, "expected_list": True, "expected_channel": False},
        {"test_name": "socket__channel", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 22", "expected_file": False, "expected_list": False, "expected_channel": True},
        {"test_name": "socket__list__channel", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 23", "expected_file": False, "expected_list": True, "expected_channel": True},
        {"test_name": "file__socket__list", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 24", "expected_file": True, "expected_list": True, "expected_channel": False},
        {"test_name": "file__socket__channel", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 25", "expected_file": True, "expected_list": False, "expected_channel": True},
        {"test_name": "file__socket__list__channel", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 26", "expected_file": True, "expected_list": True, "expected_channel": True},
        {"test_name": "wrong_file__socket", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET}, "log": "this is log 27", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_socket", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET}, "log": "this is log 28", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET}, "log": "this is log 29", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__list", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 30", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_list", "conf": {"log_file_path": ALERT_FILE,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 31", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_list", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 32", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 33", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_channel", "conf": {"log_file_path": ALERT_FILE,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 34", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 35", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__list__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 36", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_list__channel", "conf": {"log_file_path": ALERT_FILE,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 37", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "file__list__wrong_channel", "conf": {"log_file_path": ALERT_FILE,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 38", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_list__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 39", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_list__wrong_channel", "conf": {"log_file_path": ALERT_FILE,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 40", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__list__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 41", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_list__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 42", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__list", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 43", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "socket__wrong_list", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 44", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__wrong_list", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 45", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__channel", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 46", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "socket__wrong_channel", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 47", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__wrong_channel", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 48", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__list__channel", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 49", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "socket__wrong_list__channel", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 50", "expected_file": False, "expected_list": False, "expected_channel": True},
        {"test_name": "socket__list__wrong_channel", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 51", "expected_file": False, "expected_list": True, "expected_channel": False},
        {"test_name": "wrong_socket__wrong_list__channel", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 52", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "socket__wrong_list__wrong_channel", "conf": {"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 53", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__list__wrong_channel", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 54", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_socket__wrong_list__wrong_channel", "conf": {"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 55", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__list", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 56", "expected_file": False, "expected_list": True, "expected_channel": False},
        {"test_name": "file__wrong_socket__list", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 57", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "file__socket__wrong_list", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 58", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__list", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST}, "log": "this is log 59", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_socket__wrong_list", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 60", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__wrong_list", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 61", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__wrong_list", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST}, "log": "this is log 62", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 63", "expected_file": False, "expected_list": False, "expected_channel": True},
        {"test_name": "file__wrong_socket__channel", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 64", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "file__socket__wrong_channel", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 65", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 66", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "file__wrong_socket__wrong_channel", "conf": {"log_file_path": ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 67", "expected_file": True, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 68", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 69", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__list__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 70", "expected_file": False, "expected_list": True, "expected_channel": True},
        {"test_name": "wrong_file__wrong_socket__list__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 71", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__wrong_list__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 72", "expected_file": False, "expected_list": False, "expected_channel": True},
        {"test_name": "wrong_file__socket__list__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 73", "expected_file": False, "expected_list": True, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__wrong_list__channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": REDIS_ALERT_CHANNEL}, "log": "this is log 74", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__socket__wrong_list__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 75", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__list__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 76", "expected_file": False, "expected_list": False, "expected_channel": False},
        {"test_name": "wrong_file__wrong_socket__wrong_list__wrong_channel", "conf": {"log_file_path": WRONG_ALERT_FILE,"redis_socket_path": WRONG_REDIS_SOCKET,"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL}, "log": "this is log 77", "expected_file": False, "expected_list": False, "expected_channel": False}
    ]

    for i in tests:
        print_result("AlertManager: " + i["test_name"], functools.partial(test, **i))

    tests = [
        check_log_rotate,
    ]

    for i in tests:
        print_result("AlertManager: " + i.__name__, i)


def check_file_output(filter: TestFilter, log: str, expected=True) -> bool:
    file_content = filter.get_file_alerts()
    if not file_content and not expected:
        return True
    elif not file_content and expected:
        if file_content is None:
            logging.error("Was expecting alert in log file, file doesn't exist")
        else:
            logging.error("Was expecting alert in log file, file is empty")
        return False
    elif file_content and not expected:
        logging.error("Expected log file to be empty or non existing but got content")
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
        alerts = [i.decode() for i in alerts]
        if log in alerts:
            return True
        logging.error("The given alert could not be found in the Redist alert list")
        return False


def check_channel_output(pubsub, log, expected=True) -> bool:
    message = pubsub.get_message()
    if message and expected:
        if message["type"] == "message":
            alert = message["data"].decode()
            if alert != log:
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


def test(test_name: str, conf: str, log: str, expected_file=True, expected_list=True, expected_channel=True) -> bool:
    filter = TestFilter()
    if conf is not None:
        filter.configure(json.dumps(conf))

    filter.valgrind_start()

    try:
        r = redis.Redis(unix_socket_path=filter.redis.unix_socket, db=0)
        pubsub = r.pubsub()
        pubsub.subscribe([REDIS_ALERT_CHANNEL])
        sleep(0.1)
        pubsub.get_message()

        filter.send(log)
        sleep(0.5)

        if not check_file_output(filter, log, expected=expected_file):
            logging.error(test_name + ": File output check failed")
            return False
        if not check_list_output(filter, log, expected=expected_list):
            logging.error(test_name + ": Redis list output check failed")
            return False
        if not check_channel_output(pubsub, log, expected=expected_channel):
            logging.error(test_name + ": Redis channel output check failed")
            return False

        if filter.valgrind_stop is False:
            return False

        return True

    except Exception as e:
        logging.error("Got an unexpected error: " + str(e))
        return False


def check_log_rotate():
    conf = {"log_file_path": ALERT_FILE}

    filter = TestFilter()
    filter.configure(json.dumps(conf))
    filter.valgrind_start()

    try:
        log = "The errors which arise from the absence of facts are far more numerous and more durable than those which result from unsound reasoning respecting true data. Charles Babbage"
        filter.send(log)
        sleep(0.5)

        if not check_file_output(filter, log, expected=True):
            logging.error('check_log_rotate' + ": File output check failed")
            return False

        # Log Rotate
        os.rename(ALERT_FILE, ALERT_FILE + '.1')
        filter.process.send_signal(SIGHUP)

        log = "C is quirky, flawed, and an enormous success. Dennis Ritchie"
        filter.send(log)
        sleep(0.5)

        if not check_file_output(filter, log, expected=True):
            logging.error('check_log_rotate' + ": File output check failed")
            return False


        if filter.valgrind_stop is False:
            return False

        return True

    except Exception as e:
        logging.error("Got an unexpected error: " + str(e))
        return False
