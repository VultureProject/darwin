import logging
import os
import uuid
import ctypes
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

    def get_logs_from_file(self, nb_lines=-1):
        """
        Read nb_lines from the internal log file and return them.
        On error returns None.
        """
        l = None
        try:
            with open(self.log_file, 'r') as f:
                if nb_lines == -1:
                    l = f.readlines()
                else:
                    l = f.readlines(nb_lines)
        except Exception as e:
            logging.error("Error opening log file: {}".format(e))
            return None
        return l


def run():
    tests = [
        single_log_to_file,
        multiple_log_to_file,
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
    pass


def single_log_to_both():
    pass


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
    pass


def multiple_log_to_both():
    pass