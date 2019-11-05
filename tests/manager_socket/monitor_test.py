from manager_socket.utils import requests, CONF_EMPTY, CONF_FLOGS, CONF_ONE, CONF_THREE, CONF_THREE_ONE_WRONG, REQ_MONITOR, RESP_EMPTY, RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3, PATH_CONF_FLOGS
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop
from tools.output import print_result


def run():
    tests = [
        multiple_filters_running,
		multiple_filters_running_one_fail,
        one_filters_running,
        no_filter,
    ]

    for i in tests:
        print_result("Monitoring: " + i.__name__, i)


def multiple_filters_running():

    ret = False

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret

def multiple_filters_running_one_fail():

    ret = False

    darwin_configure(CONF_THREE_ONE_WRONG)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_3]):
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret

def one_filters_running():

    ret = False

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if RESP_LOGS_1 in resp:
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret

def no_filter():
    ret = False

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if RESP_EMPTY in resp:
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    return ret
