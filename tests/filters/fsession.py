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
        input_param3_cannot_parse_value,
        input_invalid_token_length,
        input_ok_no_param3,
        input_ok_param3_int,
        input_ok_param3_string,
        input_ok_multi_repo_no_match,
        input_ok_multi_repo_one_match,
        input_ok_multi_repo_multi_match,
        input_ok_wrong_repos_no_match,
        multi_input_ok_some_matches,
        no_change_to_ttls,
        change_existing_ttl,
        set_new_ttl
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



def _input_tests(test_name, data, expected_certitudes, populate_redis=None, expected_log=None):
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
                redis.hset(entry[0], entry[1], entry[2])
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

    if expected_log and not session_filter.check_line_in_filter_log(expected_log):
        logging.error("start_ok: missing error line in logfile, please check logs")
        logging.error(f"start_ok: missing \"{expected_log}\"")
        return False
    return True



def input_wrong_format():
    return _input_tests("input_wrong_format",
    data=[1],
    expected_certitudes=[101],
    expected_log="ParseBody: The input line is not an array")

def input_not_enough_parameters():
    return _input_tests("input_not_enough_parameters",
    data=[[1]],
    expected_certitudes=[101],
    expected_log="You must provide at least two arguments per request: the token and repository ID")

def input_too_much_parameters():
    return _input_tests("input_too_much_parameters",
    data=[[1, 2, 3, 4]],
    expected_certitudes=[101],
    expected_log="You must provide at most three arguments per request: the token, the repository ID and the expiration value to set to the token key")

def input_param1_wrong_type():
    return _input_tests("input_param1_wrong_type",
    data=[[1, "2", 3]],
    expected_certitudes=[101],
    expected_log="The token must be a string")

def input_param2_wrong_type():
    return _input_tests("input_param2_wrong_type",
    data=[["1", 2, 3]],
    expected_certitudes=[101],
    expected_log="The repository IDs sent must be a string in the following format: REPOSITORY1;REPOSITORY2;...")

def input_param3_wrong_type():
    return _input_tests("input_param3_wrong_type",
    data=[["1", "2", 3.14]],
    expected_certitudes=[101],
    expected_log="expiration should be a valid positive number")

def input_param3_cannot_parse_value():
    return _input_tests("input_param3_cannot_parse_value",
    data=[
        ["1", "2", "wrong"],
        ["1", "2", -1234]
    ],
    expected_certitudes=[101, 101],
    expected_log="expiration is not a valid number")

def input_invalid_token_length():
    return _input_tests("input_param3_cannot_parse_value",
    data=[["12", "2"]],
    expected_certitudes=[0],
    expected_log="Invalid token size: 2. Expected size: 64")

def input_ok_no_param3():
    return _input_tests("input_ok_param3_int",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2"]],
    expected_certitudes=[0])

def input_ok_param3_int():
    return _input_tests("input_ok_param3_int",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2", 1]],
    expected_certitudes=[0],
    expected_log="resetting token expiration to 1s")

def input_ok_param3_string():
    return _input_tests("input_ok_param3_string",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2", "42"]],
    expected_certitudes=[0],
    expected_log="resetting token expiration to 42s")

def input_ok_multi_repo_no_match():
    return _input_tests("input_ok_multi_repo_no_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2;3"]],
    expected_certitudes=[0],
    expected_log="authenticated on repository IDs 2 3 = 0")

def input_ok_multi_repo_one_match():
    return _input_tests("input_ok_multi_repo_one_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "2;3"]],
    expected_certitudes=[1],
    expected_log="authenticated on repository IDs 2 3 = 1",
    populate_redis=[
        ("1234567890123456789012345678901234567890123456789012345678901234", "2", "1")
    ])

def input_ok_multi_repo_multi_match():
    return _input_tests("input_ok_multi_repo_multi_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "1;2;3"]],
    expected_certitudes=[1],
    expected_log="authenticated on repository IDs 1 2 3 = 1",
    populate_redis=[
        ("1234567890123456789012345678901234567890123456789012345678901234", "2", "1"),
        ("1234567890123456789012345678901234567890123456789012345678901234", "3", "1")
    ])

def input_ok_wrong_repos_no_match():
    return _input_tests("input_ok_wrong_repos_no_match",
    data=[["1234567890123456789012345678901234567890123456789012345678901234", "1;2;3"]],
    expected_certitudes=[0],
    populate_redis=[
        ("1234567890123456789012345678901234567890123456789012345678901234", "4", "1"),
        ("1234567890123456789012345678901234567890123456789012345678901234", "5", "1")
    ])

def multi_input_ok_some_matches():
    return _input_tests("multi_input_ok_some_matches",
    data=[
        ["1234567890123456789012345678901234567890123456789012345678901234", "4"],
        ["1234567890123456789012345678901234567890123456789012345678901234", "1;2;3"],
        ["1234567890123456789012345678901234567890123456789012345678901234", "5"],
    ],
    expected_certitudes=[1, 0, 1],
    populate_redis=[
        ("1234567890123456789012345678901234567890123456789012345678901234", "4", "1"),
        ("1234567890123456789012345678901234567890123456789012345678901234", "5", "1")
    ])




def no_change_to_ttls():
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("no_change_to_ttls: filter didn't start correctly")
        return False

    redis = session_filter.redis.connect()
    if not redis:
        logging.error("no_change_to_ttls: could not get a valid connection to the temporary Redis for population")
        return False

    try:
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")
        redis.hset("9876543210987654321098765432109876543210987654321098765432109876", "1", "1")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876", "100")
    except Exception as e:
        logging.error("no_change_to_ttls: could not populate the temporary Redis server: {e}")
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
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") != -1:
        logging.error("no_change_to_ttls: first key shouldn't have an expiration")
        return False
    
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") > 96:
        logging.error("no_change_to_ttls: second key's expiration have been modified")
        return False
    
    return True

def change_existing_ttl():
    session_filter = Session()
    session_filter.configure()

    if not session_filter.valgrind_start():
        logging.error("change_existing_ttl: filter didn't start correctly")
        return False

    redis = session_filter.redis.connect()
    if not redis:
        logging.error("change_existing_ttl: could not get a valid connection to the temporary Redis for population")
        return False

    try:
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")
        redis.hset("9876543210987654321098765432109876543210987654321098765432109876", "1", "1")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876", "100")
    except Exception as e:
        logging.error("change_existing_ttl: could not populate the temporary Redis server: {e}")
        return False

    sleep(3)

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        [
            ["1234567890123456789012345678901234567890123456789012345678901234", "1"],
            ["9876543210987654321098765432109876543210987654321098765432109876", "1", 120]
        ],
        response_type="back",
    )

    sleep(2)

    # VERIFY Redis
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") != -1:
        logging.error("change_existing_ttl: first key shouldn't have an expiration")
        return False
    
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") < 110:
        logging.error("change_existing_ttl: second key's expiration should have been modified")
        return False
    
    return True



def set_new_ttl():
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
        redis.hset("1234567890123456789012345678901234567890123456789012345678901234", "1", "1")
        redis.hset("9876543210987654321098765432109876543210987654321098765432109876", "1", "1")
        redis.expire("9876543210987654321098765432109876543210987654321098765432109876", "100")
    except Exception as e:
        logging.error("set_new_ttl: could not populate the temporary Redis server: {e}")
        return False

    sleep(3)

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session_filter.socket,
                           socket_type="unix")

    results = darwin_api.bulk_call(
        [
            ["1234567890123456789012345678901234567890123456789012345678901234", "1", 100],
            ["9876543210987654321098765432109876543210987654321098765432109876", "1", 100]
        ],
        response_type="back",
    )

    sleep(2)

    # VERIFY Redis
    if redis.ttl("1234567890123456789012345678901234567890123456789012345678901234") < 96:
        logging.error("set_new_ttl: first key's expiration should have been modified")
        return False
    
    if redis.ttl("9876543210987654321098765432109876543210987654321098765432109876") < 96:
        logging.error("set_new_ttl: second key's expiration should have been modified")
        return False
    
    return True