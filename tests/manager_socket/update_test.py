import logging
from time import sleep
from manager_socket.utils import requests, check_filter_files, PATH_CONF_FLOGS, CONF_EMPTY, CONF_ONE, CONF_THREE, CONF_FLOGS, CONF_FLOGS_WRONG_CONF , REQ_MONITOR, REQ_UPDATE_EMPTY, REQ_UPDATE_ONE, REQ_UPDATE_TWO, REQ_UPDATE_THREE, REQ_UPDATE_NON_EXISTING, RESP_EMPTY, RESP_ONE, RESP_THREE, RESP_STATUS_OK, RESP_STATUS_KO_LIST, RESP_FILTER_NOT_EXISTING
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop
from tools.output import print_result


def run():
    tests = [
        no_filter_to_none,
        no_filter_to_one,
        no_filter_to_many,
        one_filter_to_none,
        many_filters_to_none,
        many_filters_to_one,
        one_update_none,
        one_update_one,
        one_update_one_wrong_conf,
        many_update_none,
        many_update_one,
        many_update_many,
        many_update_all,
        non_existing_filter
    ]

    for i in tests:
        print_result("Update: " + i.__name__, i())


def no_filter_to_none():
    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_UPDATE_EMPTY)
    if resp != RESP_STATUS_OK:
        logging.error("no_filter_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    return ret


def no_filter_to_one():

    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_ONE)
    if resp != RESP_STATUS_OK:
        logging.error("no_filter_to_one: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("no_filter_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def no_filter_to_many():

    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("no_filter_to_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_THREE)
    if resp != RESP_STATUS_OK:
        logging.error("no_filter_to_many: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("no_filter_to_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def one_filter_to_none():

    ret = True

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_EMPTY)    
    resp = requests(REQ_UPDATE_ONE)
    if resp != RESP_STATUS_OK:
        logging.error("one_filter_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("one_filter_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_filters_to_none():

    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_filters_to_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_EMPTY)    
    resp = requests(REQ_UPDATE_THREE)
    if resp != RESP_STATUS_OK:
        logging.error("many_filters_to_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_EMPTY:
        logging.error("many_filters_to_none: Mismatching second monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_filters_to_one():
 
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_filters_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    darwin_configure(CONF_ONE)    
    resp = requests(REQ_UPDATE_TWO)
    if resp != RESP_STATUS_OK:
        logging.error("many_filters_to_one: Update response error; got \"{}\"".format(resp))
        ret = False

    sleep(1)
    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("many_filters_to_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def one_update_none():
    
    ret = True

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_update_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay    
    resp = requests(REQ_UPDATE_EMPTY)
    if resp != RESP_STATUS_OK:
        logging.error("one_update_none: Update response error; got \"{}\"".format(resp))
        ret = False

    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_update_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def one_update_one():

    ret = True

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_update_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(2) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_ONE)
    if resp != RESP_STATUS_OK:
        logging.error("one_update_one: Update response error; got \"{}\"".format(resp))
        ret = False

    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_update_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret

def one_update_one_wrong_conf():

    ret = True

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_update_one_wrong_conf: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    sleep(2) # Need this because of the starting delay
    darwin_configure(CONF_FLOGS_WRONG_CONF, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_ONE)
    if resp not in RESP_STATUS_KO_LIST:
        logging.error("one_update_one_wrong_conf: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_ONE:
        logging.error("one_update_one_wrong_conf: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    if not check_filter_files("logs_1", ".1"):
        logging.error("Error: filter files check failed")

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_update_none():
    
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_EMPTY)
    if resp != RESP_STATUS_OK:
        logging.error("many_update_none: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_none: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_update_one():
    
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_ONE)
    if resp != RESP_STATUS_OK:
        logging.error("many_update_one: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_one: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_update_many():
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_TWO)
    if resp != RESP_STATUS_OK:
        logging.error("many_update_many: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_many: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def many_update_all():
    
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_all: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_THREE)
    if resp != RESP_STATUS_OK:
        logging.error("many_update_all: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("many_update_all: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def non_existing_filter():
    
    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("non_existing_filter: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_NON_EXISTING)
    if resp != RESP_FILTER_NOT_EXISTING:
        logging.error("non_existing_filter: Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if resp != RESP_THREE:
        logging.error("non_existing_filter: Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret
