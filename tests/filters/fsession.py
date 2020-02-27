import logging
import random
import string
import os

from tools.filter import Filter
from tools.output import print_result
from tools.redis_utils import RedisServer
from darwin import DarwinApi

DEFAULT_REDIS_SOCKET = "/tmp/redis_connection.sock"
TOKEN_LENGTH = 64 

def gen_fake_token(lenght=TOKEN_LENGTH):
    letters = string.ascii_lowercase
    return ''.join(random.choice(letters) for i in range(lenght))

class Session(Filter):
    def __init__(self, redis_server=None):
        super().__init__(filter_name="session")
        self.redis = redis_server if redis_server else RedisServer(unix_socket=DEFAULT_REDIS_SOCKET)
    
    def configure(self):
        content = '{{\n' \
                  '"redis_socket_path": "{redis_socket_path}"\n' \
                  '}}'.format(redis_socket_path=self.redis.unix_socket)
        super(Session, self).configure(content)

def run():
    tests = [
        reposIDs_exits,
        reposIDs_not_exists,
        token_bad_lenghts,
        reposIDs_bad_format
    ]

    for i in tests:
        print_result("session: " + i.__name__, i)

def reposIDs_exits():
    ret = True

    # CONFIG
    session = Session()
    session.configure()

    redis = session.redis.connect()

    token1 = gen_fake_token()
    repo1_1 = "REPO1"
    repo1_2 = "REPO2"
    repo1_3 = "REPO3"
    redis.hset(token1, repo1_1, "1")
    redis.hset(token1, repo1_2, "1")
    redis.hset(token1, repo1_3, "1")

    token2 = gen_fake_token()
    repo2 = "REPO1"
    redis.hset(token2, repo2, "1")

    # START FILTER
    if not session.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            [token1, "{}".format(repo1_1)],
            [token1, "{};{}".format(repo1_1, repo1_2)],
            [token1, "{};{};{}".format(repo1_1, repo1_2, repo1_3)],
            [token2, "{}".format(repo2)],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [1,1,1,1]
    expected_certitudes_size = len(expected_certitudes) 

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")

    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    session.clean_files()
    if not session.valgrind_stop():
        ret = False

    return ret

def reposIDs_not_exists():
    ret = True

    # CONFIG
    session = Session()
    session.configure()

    redis = session.redis.connect()

    token1 = gen_fake_token()
    repo1_1 = "REPO1"
    repo1_2 = "REPO2"
    repo1_3 = "REPO3"
    redis.hset(token1, repo1_1, "1")
    redis.hset(token1, repo1_2, "1")
    redis.hset(token1, repo1_3, "1")

    token2 = gen_fake_token()
    repo2 = "REPO1"
    redis.hset(token2, repo2, "1")

    # START FILTER
    if not session.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session.socket,
                           socket_type="unix", )

                           #NOT_AUTHENT

    results = darwin_api.bulk_call(
        [
            [token1, "UNKNOWN_REPO"],
            [token1, "{};{};UNKNOWN_REPO;{}".format(repo1_1, repo1_2, repo1_3)],
            [token2, "UNKNOWN_REPO"],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0,0,0]
    expected_certitudes_size = len(expected_certitudes)

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")

    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    session.clean_files()
    if not session.valgrind_stop():
        ret = False

    return ret

def token_bad_lenghts():
    ret = True

    # CONFIG
    session = Session()
    session.configure()

    redis = session.redis.connect()

    token1 = gen_fake_token()
    repo1_1 = "REPO1"
    repo1_2 = "REPO2"
    repo1_3 = "REPO3"
    redis.hset(token1, repo1_1, "1")
    redis.hset(token1, repo1_2, "1")
    redis.hset(token1, repo1_3, "1")

    # START FILTER
    if not session.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session.socket,
                           socket_type="unix", )

    token_lenght = int(TOKEN_LENGTH/2)
    results = darwin_api.bulk_call(
        [
            [token1[:token_lenght], "{};{};{}".format(repo1_1, repo1_2, repo1_3)],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0]
    expected_certitudes_size = len(expected_certitudes)

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")

    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    session.clean_files()
    if not session.valgrind_stop():
        ret = False

    return ret

def reposIDs_bad_format():
    ret = True

    # CONFIG
    session = Session()
    session.configure()

    redis = session.redis.connect()

    token1 = gen_fake_token()
    repo1_1 = "REPO1"
    repo1_2 = "REPO2"
    repo1_3 = "REPO3"
    redis.hset(token1, repo1_1, "1")
    redis.hset(token1, repo1_2, "1")
    redis.hset(token1, repo1_3, "1")

    # START FILTER
    if not session.valgrind_start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=session.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            [token1, ";{};{};{}".format(repo1_1, repo1_2, repo1_3)],
            [token1, "{};;{};{}".format(repo1_1, repo1_2, repo1_3)],
            [token1, "{};;;;{};{}".format(repo1_1, repo1_2, repo1_3)],
            [token1, "{};{};{};".format(repo1_1, repo1_2, repo1_3)],
            [token1, ";{};{};;;{};".format(repo1_1, repo1_2, repo1_3)],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [1,1,1,1,1]
    expected_certitudes_size = len(expected_certitudes)

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")

    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    session.clean_files()
    if not session.valgrind_stop():
        ret = False

    return ret