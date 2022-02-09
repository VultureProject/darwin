import logging
from os import kill, rename, remove
from signal import SIGHUP
from manager_socket.utils import requests, CONF_EMPTY, REQ_MONITOR
from tools.darwin_utils import darwin_configure, darwin_start, darwin_stop, darwin_remove_configuration, count_file_lines
from tools.output import print_result
from conf import TEST_FILES_DIR

MANAGER_LOGFILE = TEST_FILES_DIR + "/log/darwin_manager.log"

def run():
    tests = [
        check_write_logs,
        check_rotate_logs,
    ]

    for i in tests:
        print_result("Logging: " + i.__name__, i)


def check_write_logs():
    ret = True

    try:
        init_lines = count_file_lines(MANAGER_LOGFILE)
    except:
        init_lines = 0

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    darwin_stop(process)
    darwin_remove_configuration()

    if init_lines == count_file_lines(MANAGER_LOGFILE):
        logging.error("check_write_logs: manager didn't write a single line in expected logfile")
        ret = False

    return ret


def check_rotate_logs():
    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    rename(MANAGER_LOGFILE, MANAGER_LOGFILE + ".moved")
    kill(process.pid, SIGHUP)
    lines_after_rotate = count_file_lines(MANAGER_LOGFILE + ".moved")

    resp = requests(REQ_MONITOR)
    if resp == "":
        logging.error("check_rotate_logs: manager didn't respond to monitoring query")
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()

    if count_file_lines(MANAGER_LOGFILE + ".moved") > lines_after_rotate:
        logging.error("check_rotate_logs: new lines were written to old logfile")
        ret = False

    try:
        if count_file_lines(MANAGER_LOGFILE) == 0:
            logging.error("check_rotate_logs: no new lines were written on file after rotation")
            ret = False
    except FileNotFoundError:
        logging.error("check_rotate_logs: log file wasn't recreated by manager")
        ret = False

    remove(MANAGER_LOGFILE + ".moved")

    return ret