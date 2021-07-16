import json
import logging
from time import sleep
import os

from tools.redis_utils import RedisServer
from tools.filter import Filter
from tools.output import print_result
from conf import TEST_FILES_DIR
from darwin import DarwinApi

REDIS_SOCKET = f"{TEST_FILES_DIR}/redis_session.sock"

class Session(Filter):
    def __init__(self):
        self.redis = RedisServer(unix_socket=REDIS_SOCKET)
        super().__init__(filter_name="session")

    def configure(self, content=None):
        if not content:
            content = '{{\n' \
                        '"redis_socket_path": "{redis_socket}"' \
                    '}}'.format(redis_socket=REDIS_SOCKET)
        super(Session, self).configure(content)




def run():
    tests = [
        no_redis_no_start,
        no_valid_redis_no_start,
        start_ok,
        input_wrong_format,
        input_not_enough_parameters,
        input_too_much_parameters,
        input_param1_wrong_type,
        input_param2_wrong_type,
        input_param3_wrong_type,
        input_param3_invalid,
        input_invalid_token_length,
        input_ok_no_param3,
        input_ok_param3_int,
        input_ok_param3_string,
        input_ok_multi_repo_no_match,
        input_ok_multi_repo_one_match,
        input_ok_multi_repo_multi_match,
        input_ok_wrong_repos_no_match,
        multi_input_ok_some_matches,
        no_timeout_no_refresh,
        timeout_but_no_change_to_ttl,
        set_new_ttl,
        timeout_with_change_to_ttl
    ]

    for i in tests:
        print_result("session: " + i.__name__, i)


def no_redis_no_start():
    session_filter = Session()
    session_filter.configure("{}")

    if session_filter.valgrind_start():
        logging.error("no_redis_no_start: filter should not start without the redis_socket_path parameter")
        return False

    if not session_filter.check_line_in_filter_log(f"Missing parameter: 'redis_socket_path'"):
        logging.error("no_valid_redis_no_start: missing error line in logfile, please check logs")
        logging.error("no_valid_redis_no_start: missing \"Missing parameter: 'redis_socket_path'\"")
        return False

    return True


def no_valid_redis_no_start():
    session_filter = Session()
    session_filter.configure()

    # stop Redis
    del session_filter.redis

    if session_filter.valgrind_start():
        logging.error("no_valid_redis_no_start: filter should not start without a valid Redis server")
        return False

    if not session_filter.check_line_in_filter_log(f"could not connect to unix socket '{REDIS_SOCKET}'"):
        logging.error("no_valid_redis_no_start: missing error line in logfile, please check logs")
        logging.error(f"no_valid_redis_no_start: missing \"could not connect to unix socket '{REDIS_SOCKET}'\"")
        return False

    return True


def start_ok():
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("start_ok: filter didn't start correctly")
        return False

    if not session_filter.check_line_in_filter_log("Server::Run:: Running..."):
        logging.error("start_ok: missing error line in logfile, please check logs")
        logging.error("start_ok: missing \"Server::Run:: Running...\"")
        return False

    if not session_filter.valgrind_stop():
        logging.error("start_ok: filter didn't stop properly")
        return False

    return True



def _input_tests(test_name, data, expected_certitudes, populate_redis=None, expected_logs=[]):
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error(f"{test_name}: filter didn't start correctly")
        return False

    if populate_redis:
        redis = session_filter.redis.connect()
        if not redis:
            logging.error(f"{test_name}: could not get a valid connection to the temporary Redis for population")
            return False

        for entry in populate_redis:
            try:
                infos = entry['infos']
                key_type = entry['type']
                expiration = entry.get('expiration', 0)
                if key_type == "hset":
                    redis.hset(infos[0], infos[1], infos[2])
                elif key_type == "set":
                    redis.set(infos[0], infos[1])
                if expiration:
                    redis.expire(infos[0], expiration)
            except Exception as e:
                logging.error(f"{test_name}: could not populate the temporary Redis server: {e}")
                return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        data,
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')

    if certitudes is None:
        logging.error(f"{test_name}: No certitude list found in result".format(test_name))
        return False


    if len(certitudes) != len(expected_certitudes):
        logging.error(f"{test_name}: Unexpected certitude size of {len(certitudes)} instead of {len(expected_certitudes)}")
        return False

    if certitudes != expected_certitudes:
        logging.error(f"{test_name}: Unexpected certitude of {certitudes} instead of {expected_certitudes}")
        return False

    for expected_log in expected_logs:
        if expected_log and not session_filter.check_line_in_filter_log(expected_log, keep_init_pos=False):
            logging.error("start_ok: missing error line in logfile, please check logs")
            logging.error(f"start_ok: missing \"{expected_log}\"")
            return False
    return True



def input_wrong_format():
    return _input_tests("input_wrong_format",
    data=[1],
    expected_certitudes=[101],
    expected_logs=["ParseBody: The input line is not an array"])

def input_not_enough_parameters():
    return _input_tests("input_not_enough_parameters",
    data=[[1]],
    expected_certitudes=[101],
    expected_logs=["You must provide at least two arguments per request: the token and repository ID"])

def input_too_much_parameters():
    return _input_tests("input_too_much_parameters",
    data=[[1, 2, 3, 4]],
    expected_certitudes=[101],
    expected_logs=["You must provide at most three arguments per request: the token, the repository ID and the expiration value to set to the token key"])

def input_param1_wrong_type():
    return _input_tests("input_param1_wrong_type",
    data=[[1, "2", 3]],
    expected_certitudes=[101],
    expected_logs=["The token must be a string"])

def input_param2_wrong_type():
    return _input_tests("input_param2_wrong_type",
    data=[["1", 2, 3]],
    expected_certitudes=[101],
    expected_logs=["The repository IDs sent must be a string in the following format: REPOSITORY1;REPOSITORY2;..."])

def input_param3_wrong_type():
    return _input_tests("input_param3_wrong_type",
    data=[["1", "2", 3.14]],
    expected_certitudes=[101],
    expected_logs=["expiration should be a valid positive number"])

def input_param3_invalid():
    return _input_tests("input_param3_invalid",
    data=[
        ["1", "2", "99999999999999999999"],
        ["1", "2", 18446744073709551615], # Highest possible value for uint64
        ["1", "2", 18446744073709551616], # Highest possible value for uint64 + 1
        ["1", "2", -1234]
    ],
    expected_certitudes=[101, 0, 101, 101],
    expected_logs=[
        "expiration's value out of bounds",
        "expiration should be a valid positive number",
        "expiration should be a valid positive number"
    ])

def input_invalid_token_length():
    return _input_tests("input_invalid_token_length",
    data=[["12", "2"]],
    expected_certitudes=[0],
    expected_logs=["Invalid token size: 2. Expected size: 64"])

def input_ok_no_param3():
    return _input_tests("input_ok_no_param3",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2"]],
    expected_certitudes=[0])

def input_ok_param3_int():
    return _input_tests("input_ok_param3_int",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2", 13]],
    expected_certitudes=[0])

def input_ok_param3_string():
    return _input_tests("input_ok_param3_string",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2", "42"]],
    expected_certitudes=[0])

def input_ok_multi_repo_no_match():
    return _input_tests("input_ok_multi_repo_no_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2;3"]],
    expected_certitudes=[0],
    expected_logs=[
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_2",
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_3"
    ])

def input_ok_multi_repo_one_match():
    return _input_tests("input_ok_multi_repo_one_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2;3"]],
    expected_certitudes=[1],
    expected_logs=["Cookie 1234567890123456789012345678901234567890123456789012345678901234 authenticated on repository 2"],
    populate_redis=[
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_2", "1"), "type": "set"}
    ])

def input_ok_multi_repo_multi_match():
    return _input_tests("input_ok_multi_repo_multi_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "1;2;3"]],
    expected_certitudes=[1],
    expected_logs=[
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_1",
        "Cookie 1234567890123456789012345678901234567890123456789012345678901234 authenticated on repository 2",
    ],
    populate_redis=[
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_2", "1"), "type": "set"},
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_3", "1"), "type": "set"}
    ])

def input_ok_wrong_repos_no_match():
    return _input_tests("input_ok_wrong_repos_no_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "1;2;3"]],
    expected_certitudes=[0],
    expected_logs=[
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_1",
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_2",
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_3",
    ],
    populate_redis=[
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_4", "1"), "type": "set"},
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_5", "1"), "type": "set"}
    ])

def multi_input_ok_some_matches():
    return _input_tests("multi_input_ok_some_matches",
    data=[
        ["1234567890123456789012345678901234567890123456789012345678901234", "4"],
        ["1234567890123456789012345678901234567890123456789012345678901234", "1;2;3"],
        ["1234567890123456789012345678901234567890123456789012345678901234", "5"],
    ],
    expected_certitudes=[1, 0, 1],
    expected_logs=[
        "Cookie 1234567890123456789012345678901234567890123456789012345678901234 authenticated on repository 4",
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_1",
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_2",
        "no result for key 1234567890123456789012345678901234567890123456789012345678901234_3",
        "Cookie 1234567890123456789012345678901234567890123456789012345678901234 authenticated on repository 5",
    ],
    populate_redis=[
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_4", "1"), "type": "set"},
        {"infos": ("1234567890123456789012345678901234567890123456789012345678901234_5", "1"), "type": "set"}
    ])



def no_timeout_no_refresh():
    """
    Requests without any timeout, shouldn't be refreshed
    """
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("no_timeout_no_refresh: filter didn't start correctly")
        return False

    redis = session_filter.redis.connect()
    if not redis:
        logging.error("no_timeout_no_refresh: could not get a valid connection to the temporary Redis for population")
        return False

    try:
        redis.set("1234567890123456789012345678901234567890123456789012345678901234_1", "1")
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")

        redis.set("9876543210987654321098765432109876543210987654321098765432109876_1", "1")
        redis.hset("9876543210987654321098765432109876543210987654321098765432109876", "1", "1")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876_1", "100")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876", "100")
    except Exception as e:
        logging.error(f"no_timeout_no_refresh: could not populate the temporary Redis server: {e}")
        return False

    sleep(3)

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        [
            ["1234567890123456789012345678901234567890123456789012345678901234", "1"],
            ["9876543210987654321098765432109876543210987654321098765432109876", "1"]
        ],
        response_type="back",
    )

    sleep(2)

    # VERIFY Redis
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234_1") != -1:
        logging.error("no_timeout_no_refresh: first key shouldn't have an expiration")
        return False

    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") != -1:
        logging.error("no_timeout_no_refresh: first token shouldn't have an expiration")
        return False

    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876_1") > 96:
        logging.error("no_timeout_no_refresh: second key's expiration have been modified")
        return False
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") > 96:
        logging.error("no_timeout_no_refresh: second token's expiration have been modified")
        return False

    return True



def timeout_but_no_change_to_ttl():
    """
    Request with timeout update, but lower than the current TTL = timeout shouldn't be modified
    """

    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("timeout_but_no_change_to_ttl: filter didn't start correctly")
        return False

    redis = session_filter.redis.connect()
    if not redis:
        logging.error("timeout_but_no_change_to_ttl: could not get a valid connection to the temporary Redis for population")
        return False

    try:
        redis.set("1234567890123456789012345678901234567890123456789012345678901234_1", "1")
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")

        redis.set("9876543210987654321098765432109876543210987654321098765432109876_1", "1")
        redis.hset("9876543210987654321098765432109876543210987654321098765432109876", "1", "1")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876_1", "100")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876", "100")
    except Exception as e:
        logging.error(f"timeout_but_no_change_to_ttl: could not populate the temporary Redis server: {e}")
        return False

    sleep(3)

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        [
            ["9876543210987654321098765432109876543210987654321098765432109876", "1", 50]
        ],
        response_type="back",
    )

    sleep(2)
    # VERIFY Logs
    unexpected_logs = [
        "resetting expiration to 50"
    ]
    for unexpected_log in unexpected_logs:
        if session_filter.check_line_in_filter_log(unexpected_log, keep_init_pos=False):
            logging.error("timeout_but_no_change_to_ttl: should not have line in logfile, please check logs")
            logging.error(f"timeout_but_no_change_to_ttl: log line \"{expected_log}\"")
            return False

    # VERIFY Redis
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234_1") != -1:
        logging.error("timeout_but_no_change_to_ttl: first key shouldn't have an expiration")
        return False
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") != -1:
        logging.error("timeout_but_no_change_to_ttl: first token shouldn't have an expiration")
        return False

    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876_1") < 50:
        logging.error("timeout_but_no_change_to_ttl: second key's expiration shouldn't have been modified")
        return False
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") < 50:
        logging.error("timeout_but_no_change_to_ttl: second token's expiration shouldn't have been modified")
        return False

    return True



def set_new_ttl():
    """
    Requests with a timeout, one of the keys had no TTL, the key should have a TTL after that
    """
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("set_new_ttl: filter didn't start correctly")
        return False

    redis = session_filter.redis.connect()
    if not redis:
        logging.error("set_new_ttl: could not get a valid connection to the temporary Redis for population")
        return False

    try:
        redis.set("1234567890123456789012345678901234567890123456789012345678901234_1", "1")
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")
    except Exception as e:
        logging.error(f"set_new_ttl: could not populate the temporary Redis server: {e}")
        return False

    sleep(3)

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        [
            ["1234567890123456789012345678901234567890123456789012345678901234", "1", 54]
        ],
        response_type="back",
    )

    sleep(2)

    # VERIFY Logs
    expected_logs = [
        "resetting expiration to 54"
    ]
    for expected_log in expected_logs:
        if not session_filter.check_line_in_filter_log(expected_log, keep_init_pos=False):
            logging.error("set_new_ttl: missing error line in logfile, please check logs")
            logging.error(f"set_new_ttl: missing \"{expected_log}\"")
            return False

    # VERIFY Redis
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234_1") != -1:
        logging.error("set_new_ttl: first key should not have an expiration")
        return False
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") == -1:
        logging.error("set_new_ttl: first token should have an expiration")
        return False
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") > 54:
        logging.error("set_new_ttl: first key's expiration is over expected expiration")
        return False

    return True



def timeout_with_change_to_ttl():
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("timeout_with_change_to_ttl: filter didn't start correctly")
        return False

    redis = session_filter.redis.connect()
    if not redis:
        logging.error("timeout_with_change_to_ttl: could not get a valid connection to the temporary Redis for population")
        return False

    try:
        redis.set("1234567890123456789012345678901234567890123456789012345678901234_1", "1")
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")
        redis.expire("1234567890123456789012345678901234567890123456789012345678901234", "100")

        redis.set("9876543210987654321098765432109876543210987654321098765432109876_1", "1")
        redis.hset("9876543210987654321098765432109876543210987654321098765432109876", "1", "1")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876_1", "20")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876", "100")
    except Exception as e:
        logging.error(f"timeout_with_change_to_ttl: could not populate the temporary Redis server: {e}")
        return False

    sleep(3)

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        [
            ["1234567890123456789012345678901234567890123456789012345678901234", "1", 200],
            ["9876543210987654321098765432109876543210987654321098765432109876", "1", 120]
        ],
        response_type="back",
    )

    sleep(2)
    # VERIFY Logs
    expected_logs = [
        "resetting expiration to 200",
        "resetting expiration to 120"
    ]
    for expected_log in expected_logs:
        if not session_filter.check_line_in_filter_log(expected_log, keep_init_pos=False):
            logging.error("timeout_with_change_to_ttl: missing error line in logfile, please check logs")
            logging.error(f"timeout_with_change_to_ttl: missing \"{expected_log}\"")
            return False

    # VERIFY Redis
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234_1") != -1:
        logging.error("timeout_with_change_to_ttl: first key shouldn't have an expiration")
        return False
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") == -1:
        logging.error("timeout_with_change_to_ttl: first token should have an expiration")
        return False
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") == -1:
        logging.error("timeout_with_change_to_ttl: second token should have an expiration")
        return False

    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") < 194:
        logging.error("timeout_with_change_to_ttl: first token's expiration should have been modified")
        return False
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876_1") > 15:
        logging.error("timeout_with_change_to_ttl: first key's expiration shouldn't have been modified")
        return False
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") < 114:
        logging.error("timeout_with_change_to_ttl: second token's expiration should have been modified")
        return False

    return True
