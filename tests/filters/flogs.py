import logging
import os
import uuid
import ctypes
import redis
from time import sleep
from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi, DarwinPacket


REDIS_SOCKET = "/var/sockets/redis/redis.sock"
REDIS_LIST_NAME = "logs_darwin"
LOG_FILE = "/tmp/logs_test.log"
FLOGS_CONFIG_FILE = '{{"log_file_path": "{0}"}}'.format(LOG_FILE)
FLOGS_CONFIG_REDIS = '{{"redis_socket_path": "{0}", "redis_list_name": "{1}"}}'.format(REDIS_SOCKET, REDIS_LIST_NAME)
FLOGS_CONFIG_BOTH = '{{"redis_socket_path": "{1}", "redis_list_name": "{2}", "log_file_path": "{0}"}}'.format(LOG_FILE, REDIS_SOCKET, REDIS_LIST_NAME)


class Logs(Filter):
    def __init__(self):
        super().__init__(filter_name="logs")
        self.log_file = LOG_FILE

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

    def get_log_from_redis(self):
        res = None
        
        try:
            r = redis.Redis(host='localhost', port=6379, db=0)
            res = r.rpop(REDIS_LIST_NAME)
            r.close()
        except Exception as e:
            logging.error("Unable to connect to redis: {}".format(e))
            return None
        
        return res


def run():
    tests = [
        single_log_to_file,
        single_log_to_redis,
        single_log_to_both,
        multiple_log_to_file,
        multiple_log_to_redis,
        multiple_log_to_both,
        empty_log_to_file,
        empty_log_to_redis,
        empty_log_to_both
    ]

    for i in tests:
        print_result("logs: " + i.__name__, i())


def single_log_to_file():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_FILE)
    filter.valgrind_start()

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
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    filter.valgrind_start()

    try:
        filter.log(b'If you think your users are idiots, only idiots will use it. Linus Torvald\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    log = filter.get_log_from_redis()
    if log != b'If you think your users are idiots, only idiots will use it. Linus Torvald\n':
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    return True


def single_log_to_both():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_BOTH)
    filter.valgrind_start()

    try:
        filter.log(b'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != ['There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n']:
        logging.error("single_log_to_file: Log line not matching: {}".format(logs))
        return False

    log = filter.get_log_from_redis()
    if log != b'There are only two things wrong with C++:  The initial concept and the implementation. Bertrand Meyer\n':
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    return True


def multiple_log_to_file():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_FILE)
    filter.valgrind_start()

    try:
        filter.log(b'I define UNIX as 30 definitions of regular expressions living under one roof. Donald Knuth\n')
        filter.log(b'The computer was born to solve problems that did not exist before. Bill Gates\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != [
        'I define UNIX as 30 definitions of regular expressions living under one roof. Donald Knuth\n',
        'The computer was born to solve problems that did not exist before. Bill Gates\n'
    ]:
        logging.error("single_log_to_file: Log line not matching: {}".format(logs))
        return False

    return True


def multiple_log_to_redis():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    filter.valgrind_start()

    try:
        filter.log(b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n')
        filter.log(b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    log = filter.get_log_from_redis()
    if log != b'Computers are good at following instructions, but not at reading your mind. Donald Knuth\n':
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    log = filter.get_log_from_redis()
    if log != b'If we wish to count lines of code, we should not regard them as lines produced but as lines spent. Edsger Dijkstra\n':
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    return True


def multiple_log_to_both():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_BOTH)
    filter.valgrind_start()

    try:
        filter.log(b'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n')
        filter.log(b'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != [
        'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n',
        'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n'
        ]:
        logging.error("single_log_to_file: Log line not matching: {}".format(logs))
        return False

    log = filter.get_log_from_redis()
    if log != b'UNIX is simple.  It just takes a genius to understand its simplicity. Denis Ritchie\n':
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False
    
    log = filter.get_log_from_redis()
    if log != b'A hacker is someone who enjoys playful cleverness, not necessarily with computers. Richard Stallman\n':
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    return True


def empty_log_to_file():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_FILE)
    filter.valgrind_start()

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != []:
        logging.error("single_log_to_file: Log line not matching: {}".format(logs))
        return False

    return True


def empty_log_to_redis():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_REDIS)
    filter.valgrind_start()

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    log = filter.get_log_from_redis()
    if log is not None:
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    return True


def empty_log_to_both():
    filter = Logs()
    filter.configure(FLOGS_CONFIG_BOTH)
    filter.valgrind_start()

    try:
        filter.log(b'')
        sleep(1)
    except Exception as e:
        logging.error("single_log_to_file: Unable to connect to filter: {}".format(e))
        return False

    logs = filter.get_logs_from_file()
    if logs != []:
        logging.error("single_log_to_file: Log line not matching: {}".format(logs))
        return False

    log = filter.get_log_from_redis()
    if log is not None:
        logging.error("single_log_to_file: Log line not matching: {}".format(log))
        return False

    return True