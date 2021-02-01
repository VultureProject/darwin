import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi
from conf import TEST_FILES_DIR, PYTHON_ENV_PATH

EXPECTED_HEADER = "IP,HOSTNAME,OS,PROTO,PORT"

class Sofa(Filter):
    def __init__(self):
        super().__init__(filter_name="sofa")

    def configure(self, env=PYTHON_ENV_PATH, module="sofa_mock", function="main", test_dir=TEST_FILES_DIR):
        content = '{{\n' \
                  '"python_env_path": "{python_env_path}",\n' \
                  '"module": "{module}",\n' \
                  '"function": "{function}",\n' \
                  '"custom_python_path": "{test_path}"\n' \
                  '}}'.format(python_env_path=env,
                              module=module,
                              function=function,
                              test_path=test_dir)
        super(Sofa, self).configure(content)

    def clean_files(self):
        super(Sofa, self).clean_files()

        try:
            for root, dirs, files in os.walk("/var/tmp"):
                for file in files:
                    if file.endswith(".csv") or file.endswith(".json"):
                        os.remove(os.path.join(root, file))
        except:
            pass


def run():
    tests = [
        good_format_data_test,
        good_format_with_colons_data_test,
        data_too_short_test,
        data_too_long_test,
        data_ip_not_string_test,
        data_port_not_string_test,
        data_hotname_not_string_test,
        data_os_not_string_test,
        data_proto_not_string_test,
        python_false_return_test,
        data_bad_list_test,
        blank_return_file_test,
        no_return_file_test,
    ]

    for i in tests:
        print_result("sofa: " + i.__name__, i)


def test(data, expected_data, test_name, expect_result=True):
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

    if results is '' and expect_result:
        logging.error("Sofa test : {} : No body found in result".format(test_name))
        darwin_api.close()
        sofa_filter.clean_files()
        sofa_filter.stop()
        return False

    if expect_result:
        results = results.splitlines()
        header = results[0]
        body = results[1:]
    else:
        header = EXPECTED_HEADER
        body = []

    if header != EXPECTED_HEADER:
        ret = False
        logging.error("Sofa test: {} : Unexpected header -> {}, expected {}".format(test_name, header, EXPECTED_HEADER))

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

    # ret = hostlookup_filter.valgrind_stop() or hostlookup_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not sofa_filter.stop():
        ret = False

    for root, dirs, files in os.walk("/var/tmp"):
        for file in files:
            if file.endswith(".csv") or file.endswith(".json"):
                logging.error("Sofa test: {} : remaining file {}".format(
                    test_name,
                    file
                ))
                ret = False

    sofa_filter.clean_files()
    return ret


def good_format_data_test():
    data = [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
           ]
    return test(
        data,
        [
            '192.168.1.18,,Linux 2.6.39,tcp,80',
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "good_format_data_test"
        )


def good_format_with_colons_data_test():
    data = [
            ["192.168.1.18", "", "Linux, 2.6.39, hello", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
           ]
    return test(
        data,
        [
            '192.168.1.18,,"Linux, 2.6.39, hello",tcp,80',
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "good_format_with_colons_data_test"
        )


def data_too_short_test():
    return test(
        [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp"],  # Data too short
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
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
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "data_too_long_test"
        )


def data_port_not_string_test():
    return test(
        [
            ["192.168.1.18", "", "Linux 2.6.39", "tcp", 80],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
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
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "data_ip_not_string_test"
        )

def data_hotname_not_string_test():
    return test(
        [
            ["192.168.1.31", 42, "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "data_hotname_not_string_test"
        )

def data_os_not_string_test():
    return test(
        [
            ["192.168.1.31", "", 2.6, "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "data_os_not_string_test"
        )

def data_proto_not_string_test():
    return test(
        [
            ["192.168.1.31", "", "Linux 2.6.39", 42, "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [
            '192.168.1.31,,Microsoft Windows Server 2003 R2,tcp,21'
        ],
        "data_proto_not_string_test"
        )


def python_false_return_test():

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
        [
            ["192.168.1.31", "trigger_false", "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        filter_code="SOFA",
        response_type="back",
    )

    # VERIFY RESULTS
    results = results.get('body')

    if results is not '':
        logging.error("Sofa test : python_false_return_test : expected no result but got {} instead".format(results))
        ret = False

    # CLEAN
    darwin_api.close()

    sofa_filter.clean_files()
    # ret = hostlookup_filter.valgrind_stop() or hostlookup_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not sofa_filter.stop():
        ret = False

    # Test if the filter didn't crash and can still process entries
    if ret and not good_format_data_test():
        ret = False

    return ret


def data_bad_list_test():
    return test(
        ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"],
        [],
        "data_bad_list_test"
        )

def blank_return_file_test():
    return test(
        [
            ["192.168.1.31", "trigger_no_write", "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [],
        "blank_return_file_test"
    )

def no_return_file_test():
    return test(
        [
            ["192.168.1.31", "trigger_no_out_file", "Linux 2.6.39", "tcp", "80"],
            ["192.168.1.31", "", "Microsoft Windows Server 2003 R2", "tcp", "21"]
        ],
        [],
        "no_return_file_test",
        expect_result=False
    )