import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi
from conf import TEST_PATH, PYTHON_ENV_PATH


class Sofa(Filter):
    def __init__(self):
        super().__init__(filter_name="sofa")

    def configure(self):
        content = '{{\n' \
                  '"python_env_path": "{python_env_path}",\n' \
                  '"module": "{module}",\n' \
                  '"function": "{function}",\n' \
                  '"custom_python_path": "{test_path}tools/"\n' \
                  '}}'.format(python_env_path=PYTHON_ENV_PATH,
                              module="sofa_mock",
                              function="main",
                              test_path=TEST_PATH)
        super(Sofa, self).configure(content)

    def clean_files(self):
        super(Sofa, self).clean_files()

        try:
            os.remove(self.init_data_path)
        except:
            pass


def run():
    tests = [
        good_format_data_test,
        data_too_short_test,
        data_too_long_test,
        data_ip_not_string_test,
        data_port_not_string_test,
        data_bad_list_test
    ]

    for i in tests:
        print_result("sofa: " + i.__name__, i)


def test(data, expected_data, test_name):
    ret = True

    # CONFIG
    sofa_filter = Sofa()
    sofa_filter.configure()

    # START FILTER
    if not sofa_filter.start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=sofa_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        data,
        filter_code="SOFA",
        response_type="back",
    )

    # VERIFY RESULTS
    results = results.get('body')

    if results is None:
        logging.error("Sofa test : {} : No body found in result").format(test_name)
        darwin_api.close()
        sofa_filter.clean_files()
        sofa_filter.stop()
        return False

    results = results.splitlines()
    results = results[1:]
    body = []
    for r in results:
        body += [r.split(',')]

    expected_body_size = len(expected_data)
    if len(body) != expected_body_size:
        ret = False
        logging.error("Sofa test : {} : Unexpected body size of {} instead of {}"
                      .format(test_name, len(body), expected_body_size))

    if body != expected_data:
        ret = False
        logging.error("Sofa test : {} : Unexpected body {} instead of {}"
                      .format(test_name, body, expected_data))

    # CLEAN
    darwin_api.close()

    sofa_filter.clean_files()
    # ret = hostlookup_filter.valgrind_stop() or hostlookup_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not sofa_filter.stop():
        ret = False

    return ret


def good_format_data_test():
    data = [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
           ]
    return test(
        data,
        data,
        "good_format_data_test"
        )


def data_too_short_test():
    return test(
        [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp"],  # Data too short
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        "data_too_short_test"
        )


def data_too_long_test():
    return test(
        [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp", "80", "80"],  # Data too long
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        "data_too_short_test"
        )


def data_port_not_string_test():
    return test(
        [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp", 80],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        "data_port_not_string_test"
        )


def data_ip_not_string_test():
    return test(
        [
            [192, "", "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        "data_ip_not_string_test"
        )


def data_bad_list_test():
    return test(
        ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"],
        [],
        "data_bad_list_test"
        )
