from manager_socket.utils import requests, CONF_EMPTY, CONF_FLOGS, CONF_ONE, CONF_THREE, REQ_MONITOR, RESP_EMPTY, RESP_ONE, RESP_THREE, PATH_CONF_FLOGS
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop
from tools.output import print_result


def run():
    tests = [
        multiple_filters_running,
        one_filters_running,
        no_filter,
    ]

    for i in tests:
        print_result("Monitoring: " + i.__name__, i())


def multiple_filters_running():

    ret = False

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp == RESP_THREE:
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
    if resp == RESP_ONE:
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
    if resp == RESP_EMPTY:
        ret = True

    darwin_stop(process)
    darwin_remove_configuration()
    return ret
