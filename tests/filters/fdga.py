import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

MODEL_PATH = "/tmp/model.pb"
TOKEN_MAP_PATH = "/tmp/tokens.csv"
MAX_TOKENS = 150

class DGA(Filter):
    def __init__(self):
        super().__init__(filter_name="dga")
        self.token_map_path = TOKEN_MAP_PATH

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
            os.remove(self.token_map_path)
        except:
            pass


def run():
    tests = [
        good_format_tokens_test,
        bad_format_tokens_test,
    ]

    for i in tests:
        print_result("dga: " + i.__name__, i)

def good_format_tokens_test():
    ret = True

    # CONFIG
    dga_filter = DGA()
    dga_filter.init_tokens(["a,1",
                            "b,2",
                            "c,3",
                            "d,4"])
    dga_filter.configure()

    # START FILTER
    if not dga_filter.start():
        return False

    if not dga_filter.stop():
        ret = False

    dga_filter.clean_files()

    return ret

def bad_format_tokens_test():
    ret = True

    # CONFIG
    dga_filter = DGA()
    dga_filter.init_tokens(["a,1",
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

    dga_filter.clean_files()

    return ret
