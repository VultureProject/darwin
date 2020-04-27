import logging
import os
import uuid
import ctypes
import redis
from tools.redis_utils import RedisServer
from time import sleep
from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi, DarwinPacket


REDIS_SOCKET = "/tmp/redis_logs.sock"
REDIS_LIST_NAME = "logs_darwin"
REDIS_CHANNEL_NAME = "darwin.test"
LOG_FILE = "/tmp/logs_test.log"
FLOGS_CONFIG_FILE = '{{"log_file_path": "{0}"}}'.format(LOG_FILE)
FLOGS_CONFIG_REDIS = '{{"redis_socket_path": "{0}", "redis_list_name": "{1}"}}'.format(REDIS_SOCKET, REDIS_LIST_NAME)
FLOGS_CONFIG_REDIS_CHANNEL = '{{"redis_socket_path": "{0}", "redis_channel_name": "{1}"}}'.format(REDIS_SOCKET, REDIS_CHANNEL_NAME)
FLOGS_CONFIG_ALL = """{{"redis_socket_path": "{1}",
"redis_list_name": "{2}",
"log_file_path": "{0}",
"redis_channel_name": "{3}"}}""".format(LOG_FILE, REDIS_SOCKET, REDIS_LIST_NAME, REDIS_CHANNEL_NAME)


class Logs(Filter):
    def __init__(self, log_file=None):
        super().__init__(filter_name="logs")
        self.log_file = log_file if log_file else LOG_FILE
        try:
            os.remove(self.log_file)
        except:
            pass

    def clean_files(self):
        super(Logs, self).clean_files()

        try:
            os.remove(self.log_file)
        except:
            pass

    def log(self, log_line):
        """
        Send a single log line.
        """
        header = DarwinPacket(
            packet_type="other",
            response_type="no",
            filter_code=0x4C4F4753,
            event_id=uuid.uuid4().hex,
            body_size=len(log_line)
        )

        api = DarwinApi(socket_type="unix", socket_path=self.socket)

        api.socket.sendall(header)
        api.socket.sendall(log_line)
        api.close()

    def get_logs_from_file(self):
        """
        Read lines from the internal log file and return them.
        On error returns None.
        """
        l = None
        try:
            with open(self.log_file, 'r') as f:
                l = f.readlines()
        except Exception as e:
            logging.error("Error opening log file: {}".format(e))
            return None
        return l

    def __del__(self):
        if self.pubsub is not None:
            self.pubsub.close()
        super().__del__()

    def get_log_from_redis(redis, list):
        res = None

        try:
            with redis.connect() as redis_connection:
                res = redis_connection.rpop(list).decode('utf-8')
        except Exception as e:
            logging.error("Unable to connect to redis: {}".format(e))
            return None

        return res


def run():
    tests = [
        single_log_to_file,
        single_log_to_redis,
        single_log_to_redis_channel,
        single_log_to_all,
        multiple_log_to_file,
        multiple_log_to_redis,
        multiple_log_to_redis_channel,
        multiple_log_to_all,
        empty_log_to_file,
        empty_log_to_redis,
        empty_log_to_redis_channel,
        empty_log_to_all
    ]

    for i in tests:
        print_result("logs: " + i.__name__, i)


def single_log_to_file():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_FILE)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'Science is what we understand well enough to explain to a computer. Art is everything else we do. Donald Knuth\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != ['Science is what we understand well enough to explain to a computer. Art is everything else we do. Donald Knuth\n']:
        logging.error("single_log_to_file: Log line not matching: {}".format(logs))
        return False

    return True


def single_log_to_redis():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'If you think your users are idiots, only idiots will use it. Linus Torvald\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_redis: Unable to connect to filter: {}".format(e))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    if log != 'If you think your users are idiots, only idiots will use it. Linus Torvald\n':
        logging.error("single_log_to_redis: Log line not matching: {}".format(log))
        return False

    return True

def single_log_to_redis_channel():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS_CHANNEL)
    if not filter.valgrind_start():
        return False

    redis_server.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'Surveillance is the business model of the Internet. Bruce Schneier\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_redis_channel: Unable to connect to filter: {}".format(e))
        return False

    log = redis_server.channel_get_message()
    if log != 'Surveillance is the business model of the Internet. Bruce Schneier\n':
        logging.error("single_log_to_redis_channel: Log line not matching: {}".format(log))
        return False

    return True


def single_log_to_all():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_ALL)
    if not filter.valgrind_start():
        return False

    redis_server.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_all: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != ['There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n']:
        logging.error("single_log_to_all: Log line not matching: {}".format(logs))
        return False

    redis_log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    if redis_log != 'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n':
        logging.error("single_log_to_all: Log line not matching: {}".format(log))
        return False

    redis_channel_log = redis_server.channel_get_message()
    if redis_channel_log != 'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n':
        logging.error("single_log_to_all: Log line not matching: {}".format(log))
        return False

    return True


def multiple_log_to_file():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_FILE)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'I define UNIX as 30 definitions of regular expressions living under one roof. Donald Knuth\n')
        filter.log(b'The computer was born to solve problems that did not exist before. Bill Gates\n')
        sleep(1)
    except Exception as e:
        logging.error("multiple_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != [
        'I define UNIX as 30 definitions of regular expressions living under one roof. Donald Knuth\n',
        'The computer was born to solve problems that did not exist before. Bill Gates\n'
    ]:
        logging.error("multiple_log_to_file: Log line not matching: {}".format(logs))
        return False

    return True


def multiple_log_to_redis():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n')
        filter.log(b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n')
        sleep(1)
    except Exception as e:
        logging.error("multiple_log_to_redis: Unable to connect to filter: {}".format(e))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    if log != 'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n':
        logging.error("multiple_log_to_redis: Log line not matching: {}".format(log))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    if log != 'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n':
        logging.error("multiple_log_to_redis: Log line not matching: {}".format(log))
        return False

    return True

def multiple_log_to_redis_channel():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS_CHANNEL)
    if not filter.valgrind_start():
        return False

    redis_server.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n')
        filter.log(b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n')
        sleep(1)
    except Exception as e:
        logging.error("multiple_log_to_redis_channel: Unable to connect to filter: {}".format(e))
        return False

    log = redis_server.channel_get_message()
    if log != 'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n':
        logging.error("multiple_log_to_redis_channel: Log line not matching: {}".format(log))
        return False

    log = redis_server.channel_get_message()
    if log != 'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n':
        logging.error("multiple_log_to_redis_channel: Log line not matching: {}".format(log))
        return False

    log = redis_server.channel_get_message()
    if log is not None:
        logging.error("multiple_log_to_redis_channel: Log line not matching: waited '' but got '{}'".format(log))
        return False

    return True

def multiple_log_to_all():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_ALL)
    if not filter.valgrind_start():
        return False

    redis_server.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n')
        filter.log(b'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n')
        sleep(1)
    except Exception as e:
        logging.error("multiple_log_to_all: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != [
        'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n',
        'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n'
        ]:
        logging.error("multiple_log_to_all: Log line not matching: {}".format(logs))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    channel_log = redis_server.channel_get_message()
    if log != 'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n':
        logging.error("multiple_log_to_all: Log line not matching: {}".format(log))
        return False
    if log != channel_log:
        logging.error("multiple_log_to_all: channel log line not matching: {}".format(channel_log))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    channel_log = redis_server.channel_get_message()
    if log != 'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n':
        logging.error("multiple_log_to_all: Log line not matching: {}".format(log))
        return False
    if log != channel_log:
        logging.error("multiple_log_to_all: channel log line not matching: {}".format(channel_log))
        return False

    return True


def empty_log_to_file():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_FILE)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("empty_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != []:
        logging.error("empty_log_to_file: Log line not matching: {}".format(logs))
        return False

    return True


def empty_log_to_redis():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("empty_log_to_redis: Unable to connect to filter: {}".format(e))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    if log is not None:
        logging.error("empty_log_to_redis: Log line not matching: {}".format(log))
        return False

    return True

def empty_log_to_redis_channel():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    redis_server.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("empty_log_to_redis_channel: Unable to connect to filter: {}".format(e))
        return False

    log = redis_server.channel_get_message()
    if log is not None:
        logging.error("empty_log_to_redis_channel: Log line not matching: {}".format(log))
        return False

    return True

def empty_log_to_all():
    redis_server = RedisServer(unix_socket=REDIS_SOCKET)
    filter = Logs()
    filter.configure(FLOGS_CONFIG_ALL)
    if not filter.valgrind_start():
        return False

    redis_server.channel_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("empty_log_to_all: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != []:
        logging.error("empty_log_to_all: Log line not matching: {}".format(logs))
        return False

    log = Logs.get_log_from_redis(redis_server, REDIS_LIST_NAME)
    if log is not None:
        logging.error("empty_log_to_all: Log line not matching: {}".format(log))
        return False

    channel_log = redis_server.channel_get_message()
    if channel_log is not None:
        logging.error("empty_log_to_all: channel log line not matching: {}".format(channel_log))
        return False

    return True