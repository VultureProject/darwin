import json
import logging
import os
import math
from darwin import DarwinApi, darwinexceptions

from tools.filter import Filter
from tools.output import print_result

PASSING_TESTS_DATA = "filters/data/dga_domains_score.json"
MODEL_PATH = "filters/data/dga_model.tflite"
TOKEN_MAP_PATH = "filters/data/dga_tokens.csv"

TMP_TOKEN_MAP_PATH = f"{TOKEN_MAP_PATH}.tmp"
MAX_TOKENS = 80

class DGA(Filter):
    def __init__(self, tokens=None):
        super().__init__(filter_name="dga")
        if tokens is None:
            self.token_map_path = TOKEN_MAP_PATH
        else:
            self.token_map_path = TMP_TOKEN_MAP_PATH
            self.init_tokens(tokens)

    def configure(self):
        content = '{{\n' \
                  '"model_path": "{model_path}",\n' \
                  '"token_map_path": "{token_map_path}",\n' \
                  '"max_tokens": {max_tokens}\n' \
                  '}}'.format(model_path=MODEL_PATH, token_map_path=self.token_map_path, max_tokens=MAX_TOKENS)
        super(DGA, self).configure(content)

    def init_tokens(self, data):
        with open(self.token_map_path, mode='w') as file:
            for d in data:
                file.write(d+"\n")

    def clean_files(self):
        super(DGA, self).clean_files()

        try:
            os.remove(TMP_TOKEN_MAP_PATH)
        except:
            pass


def run():
    tests = [
        passing_tests_bulk,
        passing_tests_singles,
        good_format_tokens_test,
        bad_format_tokens_test,
    ]

    for i in tests:
        print_result("dga: " + i.__name__, i)

def passing_tests_bulk():
    dga_filter = DGA()
    dga_filter.configure()

    if not os.path.exists(PASSING_TESTS_DATA):
        logging.error(f"passing_tests_bulk Test : no data to test, file {PASSING_TESTS_DATA} does not exist")
        return False
    
    with open(PASSING_TESTS_DATA, 'r') as f:
        data = json.load(f)

    if not dga_filter.valgrind_start():
        logging.error("DGA passing_tests_bulk Test : filter did not start")
        return False
    
    darwin_api = DarwinApi(socket_path=dga_filter.socket,
                           socket_type="unix",
                           # This timeout is arbitrary, you may have to increase it on slow systems
                           timeout=40)
    try:
        results = darwin_api.bulk_call(
            [[domain] for domain in data.keys()],
            response_type="back"
        )
    except darwinexceptions.DarwinTimeoutError as e:
        logging.error(f"DGA passing_tests_bulk Test : Timeout error, it is a long operation, you may have to increase the timeout set in the darwinapi",
                    exc_info=e)
        return False
    
    # VERIFY RESULTS
    certitudes = results.get('certitude_list', None)
    if certitudes is None:
        logging.error("DGA passing_tests_bulk Test : No certitude list found in result")
        return False

    expected_values = list(data.values())

    if len(certitudes) != len(expected_values):
        logging.error(f"DGA passing_tests_bulk Test : Unexpected certitude size of {len(certitudes)} instead of {len(expected_values)}")
        return False
    ret = True
    for result, expected in zip(certitudes, expected_values):
        expected_percent = expected*100
        if not math.isclose(result, expected_percent, rel_tol=0.01, abs_tol=1):
            ret = False
            logging.error(f"DGA passing_tests_bulk Test : Unexpected certitude of {result} instead of {expected_percent}")

    return ret

def passing_tests_singles():
    dga_filter = DGA()
    dga_filter.configure()

    if not os.path.exists(PASSING_TESTS_DATA):
        logging.error(f"passing_tests_singles Test : no data to test, file {PASSING_TESTS_DATA} does not exist")
        return False
    
    with open(PASSING_TESTS_DATA, 'r') as f:
        data = json.load(f)

    if not dga_filter.valgrind_start():
        logging.error("DGA passing_tests_singles Test : filter did not start")
        return False
    
    darwin_api = DarwinApi(socket_path=dga_filter.socket,
                           socket_type="unix")
    ret = True
    for domain, expected_value in data.items():
        expected_percent = expected_value*100
        result = darwin_api.call([domain], response_type='back')

        if not math.isclose(result, expected_percent, rel_tol=0.01, abs_tol=1):
            ret = False
            logging.error(f"DGA passing_tests_singles Test : Unexpected certitude of {result} instead of {expected_percent}")
    return ret


def good_format_tokens_test():
    ret = True

    # CONFIG
    dga_filter = DGA(tokens=["a,1",
                            "b,2",
                            "c,3",
                            "d,4"])
    dga_filter.configure()

    # START FILTER
    if not dga_filter.start():
        return False

    if not dga_filter.stop():
        ret = False

    return ret

def bad_format_tokens_test():
    ret = True

    # CONFIG
    dga_filter = DGA(tokens=["a,1",
                            "b2",
                            "c,3",
                            "d,4"])
    dga_filter.configure()

    # START FILTER
    dga_filter.start()

    # THE FILTER HAVE TO QUIT GRACEFULLY, SO REMOVE ITS PID
    if os.path.exists(dga_filter.pid):
        logging.error("DGA test : Filter crash when token map given is not well formatted")
        ret = False

    return ret
