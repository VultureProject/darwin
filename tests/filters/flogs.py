import logging 
import os
import uuid
import ctypes
import redis

from time import sleep
from tools.filter import Filter
from tools.output import print_result
from tools.logger import CustomAdapter
from tools.redis_utils import RedisServer
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
    def __init__(self, logger, log_file=None, redis_server=None, nb_threads=1):
        super().__init__(filter_name="logs", nb_thread=nb_threads, logger=logger)
        self.log_file = log_file if log_file else LOG_FILE
        self.redis = redis_server if redis_server else RedisServer(unix_socket=REDIS_SOCKET)
        self.redis.start()
        self.pubsub = None
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
            self.logger.error("Error opening log file: {}".format(e))
            return None
        return l

    def get_log_from_redis(self):
        res = None

        try:
            r = self.redis.connect()
            res = r.rpop(REDIS_LIST_NAME)
            r.close()
        except Exception as e:
            self.logger.error("Unable to connect to redis: {}".format(e))
            return None

        return res

    def redis_subscribe(self, channel):
        r = self.redis.connect()
        self.pubsub = r.pubsub(ignore_subscribe_messages=True)
        self.pubsub.subscribe(channel)
        sleep(0.1)
        self.pubsub.get_message()

    def redis_channel_get_message(self):
        ret = None
        if self.pubsub is not None:
            try:
                ret = self.pubsub.get_message().get('data')
            except:
                pass
        return ret

    def __del__(self):
        if self.pubsub is not None:
            self.pubsub.close()
        self.redis.stop()
        super().__del__()

def run():
    global logger
    glogger = logging.getLogger("LOGS")

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
        logger = CustomAdapter(glogger, {'test_name': i.__name__})
        print_result("logs: " + i.__name__, i)


def single_log_to_file():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_FILE)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'Science is what we understand well enough to explain to a computer. Art is everything else we do. Donald Knuth\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != ['Science is what we understand well enough to explain to a computer. Art is everything else we do. Donald Knuth\n']:
        logger.error("Log line not matching: {}".format(logs))
        return False

    return True


def single_log_to_redis():

    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'If you think your users are idiots, only idiots will use it. Linus Torvald\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    log = filter.get_log_from_redis()
    if log != b'If you think your users are idiots, only idiots will use it. Linus Torvald\n':
        logger.error("Log line not matching: {}".format(log))
        return False

    return True

def single_log_to_redis_channel():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_REDIS_CHANNEL)
    if not filter.valgrind_start():
        return False

    filter.redis_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'Surveillance is the business model of the Internet. Bruce Schneier\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    log = filter.redis_channel_get_message()
    if log != b'Surveillance is the business model of the Internet. Bruce Schneier\n':
        logger.error("Log line not matching: {}".format(log))
        return False

    return True


def single_log_to_all():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_ALL)
    if not filter.valgrind_start():
        return False

    filter.redis_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != ['There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n']:
        logger.error("Log line not matching: {}".format(logs))
        return False

    redis_log = filter.get_log_from_redis()
    if redis_log != b'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n':
        logger.error("Log line not matching: {}".format(redis_log))
        return False

    redis_channel_log = filter.redis_channel_get_message()
    if redis_channel_log != b'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n':
        logger.error("Log line not matching: {}".format(redis_channel_log))
        return False

    return True


def multiple_log_to_file():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_FILE)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'I define UNIX as 30 definitions of regular expressions living under one roof. Donald Knuth\n')
        filter.log(b'The computer was born to solve problems that did not exist before. Bill Gates\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != [
        'I define UNIX as 30 definitions of regular expressions living under one roof. Donald Knuth\n',
        'The computer was born to solve problems that did not exist before. Bill Gates\n'
    ]:
        logger.error("Log line not matching: {}".format(logs))
        return False

    return True


def multiple_log_to_redis():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n')
        filter.log(b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    log = filter.get_log_from_redis()
    if log != b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n':
        logger.error("Log line not matching: {}".format(log))
        return False

    log = filter.get_log_from_redis()
    if log != b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n':
        logger.error("Log line not matching: {}".format(log))
        return False

    return True

def multiple_log_to_redis_channel():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_REDIS_CHANNEL)
    if not filter.valgrind_start():
        return False

    filter.redis_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n')
        filter.log(b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    log = filter.redis_channel_get_message()
    if log != b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n':
        logger.error("Log line not matching: {}".format(log))
        return False

    log = filter.redis_channel_get_message()
    if log != b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n':
        logger.error("Log line not matching: {}".format(log))
        return False

    log = filter.redis_channel_get_message()
    if log is not None:
        logger.error("Log line not matching: waited '' but got '{}'".format(log))
        return False

    return True

def multiple_log_to_all():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_ALL)
    if not filter.valgrind_start():
        return False

    filter.redis_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n')
        filter.log(b'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != [
        'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n',
        'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n'
        ]:
        logger.error("Log line not matching: {}".format(logs))
        return False

    log = filter.get_log_from_redis()
    channel_log = filter.redis_channel_get_message()
    if log != b'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n':
        logger.error("Log line not matching: {}".format(log))
        return False
    if log != channel_log:
        logger.error("channel log line not matching: {}".format(channel_log))
        return False

    log = filter.get_log_from_redis()
    channel_log = filter.redis_channel_get_message()
    if log != b'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n':
        logger.error("Log line not matching: {}".format(log))
        return False
    if log != channel_log:
        logger.error("channel log line not matching: {}".format(channel_log))
        return False

    return True


def empty_log_to_file():
    
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_FILE)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != []:
        logger.error("Log line not matching: {}".format(logs))
        return False

    return True


def empty_log_to_redis():
    
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    log = filter.get_log_from_redis()
    if log is not None:
        logger.error("Log line not matching: {}".format(log))
        return False

    return True

def empty_log_to_redis_channel():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_REDIS)
    if not filter.valgrind_start():
        return False

    filter.redis_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    log = filter.redis_channel_get_message()
    if log is not None:
        logger.error("Log line not matching: {}".format(log))
        return False

    return True

def empty_log_to_all():
    filter = Logs(logger)
    filter.configure(FLOGS_CONFIG_ALL)
    if not filter.valgrind_start():
        return False

    filter.redis_subscribe(REDIS_CHANNEL_NAME)

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logger.error("Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != []:
        logger.error("Log line not matching: {}".format(logs))
        return False

    log = filter.get_log_from_redis()
    if log is not None:
        logger.error("Log line not matching: {}".format(log))
        return False

    channel_log = filter.redis_channel_get_message()
    if channel_log is not None:
        logger.error("channel log line not matching: {}".format(channel_log))
        return False

    return True