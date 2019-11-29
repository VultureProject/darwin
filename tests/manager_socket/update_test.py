import logging
from time import sleep
from manager_socket.utils import requests, check_filter_files, PATH_CONF_FLOGS, CONF_EMPTY, CONF_ONE, CONF_THREE, CONF_FLOGS, CONF_FLOGS_WRONG_CONF , REQ_MONITOR, REQ_UPDATE_EMPTY, REQ_UPDATE_ONE, REQ_UPDATE_TWO, REQ_UPDATE_THREE, REQ_UPDATE_NON_EXISTING, RESP_EMPTY, RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3, RESP_STATUS_OK, RESP_STATUS_KO, RESP_ERROR_FILTER_NOT_EXISTING
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop
from tools.logger import CustomAdapter
from tools.output import print_result


def run():
    global logger
    glogger = logging.getLogger("UPDATE")

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
        many_update_two_wrong_conf,
        many_update_all,
        many_update_all_wrong_conf,
        non_existing_filter
    ]

    for i in tests:
        logger = CustomAdapter(glogger, {'test_name': i.__name__})
        print_result("Update: " + i.__name__, i)


def no_filter_to_none():
    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if RESP_EMPTY not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_UPDATE_EMPTY)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if RESP_EMPTY not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    return ret


def no_filter_to_one():

    ret = True

    darwin_configure(CONF_EMPTY)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if RESP_EMPTY not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_ONE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_ONE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if RESP_EMPTY not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_THREE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_EMPTY)    
    resp = requests(REQ_UPDATE_ONE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if RESP_EMPTY not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_configure(CONF_EMPTY)    
    resp = requests(REQ_UPDATE_THREE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if RESP_EMPTY not in resp:
        logger.error("Mismatching second monitor response; got \"{}\"".format(resp))
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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    darwin_configure(CONF_ONE)    
    resp = requests(REQ_UPDATE_TWO)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    sleep(1)
    resp = requests(REQ_MONITOR)
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay    
    resp = requests(REQ_UPDATE_EMPTY)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_MONITOR)
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(2) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_ONE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_MONITOR)
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    sleep(2) # Need this because of the starting delay
    darwin_configure(CONF_FLOGS_WRONG_CONF, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_ONE)
    if RESP_STATUS_KO not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if RESP_LOGS_1 not in resp:
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    if not check_filter_files("logs_1", ".1"):
        logger.error("filter files check failed")
        ret = False

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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_EMPTY)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_ONE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_TWO)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret

def many_update_two_wrong_conf():

    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    sleep(2) # Need this because of the starting delay
    darwin_configure(CONF_FLOGS_WRONG_CONF, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_TWO)
    if RESP_STATUS_KO not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    if not check_filter_files("logs_2", ".1"):
        logger.error("filter files check failed")
        ret = False

    if not check_filter_files("logs_3", ".1"):
        logger.error("filter files check failed")
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret

def many_update_all_wrong_conf():

    ret = True

    darwin_configure(CONF_THREE)
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    sleep(2) # Need this because of the starting delay
    darwin_configure(CONF_FLOGS_WRONG_CONF, path=PATH_CONF_FLOGS)
    resp = requests(REQ_UPDATE_THREE)
    if RESP_STATUS_KO not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    if not check_filter_files("logs_1", ".1"):
        logger.error("filter files check failed")
        ret = False

    if not check_filter_files("logs_2", ".1"):
        logger.error("filter files check failed")
        ret = False

    if not check_filter_files("logs_3", ".1"):
        logger.error("filter files check failed")
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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_THREE)
    if RESP_STATUS_OK not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
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
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False
    
    sleep(1) # Need this beacause of the starting delay
    resp = requests(REQ_UPDATE_NON_EXISTING)
    if RESP_ERROR_FILTER_NOT_EXISTING not in resp:
        logger.error("Update response error; got \"{}\"".format(resp))
        ret = False

    resp = requests(REQ_MONITOR)
    if not all(x in resp for x in [RESP_LOGS_1, RESP_LOGS_2, RESP_LOGS_3]):
        logger.error("Mismatching monitor response; got \"{}\"".format(resp))
        ret = False

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret
