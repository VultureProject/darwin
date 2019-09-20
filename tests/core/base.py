import logging
from os import kill, remove
from tools.filter import Filter
from tools.output import print_result
from core.utils import DEFAULT_PATH


def run():
    tests = [
        check_start_stop,
    ]

    for i in tests:
        print_result("Basic tests: " + i.__name__, i())

def check_start_stop():
    filter = Filter(DEFAULT_PATH)

    filter.configure('{"log_file_path": "/tmp/logs_test.log"}')
    filter.start()
    try:
        kill(filter.process.pid, 0)
    except OSError as e:
        logging.error("check_start_stop: Process {} not running: {}".format(filter.process.pid, e))
        return False

    if filter.stop() is not True:
        return False
    
    return True


