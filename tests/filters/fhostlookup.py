import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class HostLookup(Filter):
    def __init__(self):
        super().__init__(filter_name="hostlookup")
        self.database = "/tmp/database.txt".format(self.filter_name)

    def configure(self):
        content = '{{\n' \
                  '"database": "{database}"\n' \
                  '}}'.format(database=self.database)
        super(HostLookup, self).configure(content)

    def init_data(self, data):
        with open(self.database, mode='w') as file:
            for d in data:
                file.write(d+"\n")

    def clean_files(self):
        super(HostLookup, self).clean_files()

        try:
            os.remove(self.init_data_path)
        except:
            pass


def run():
    tests = [
        unique_untrusted_host_test,
        unique_trusted_host_test,
        multiple_trusted_host_test,
        multiple_untrusted_host_test,
        multiple_host_test,
        init_data_test
    ]

    for i in tests:
        print_result("connection: " + i.__name__, i())


def test(test_name, init_data, data, expected_certitudes, expected_certitudes_size):
    ret = True

    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data(init_data)
    hostlookup_filter.configure()

    # START FILTER
    hostlookup_filter.valgrind_start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=hostlookup_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        data,
        filter_code="HOSTLOOKUP",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')

    if certitudes is None:
        ret = False
        logging.error("Hostlookup Test : {} : No certitude list found in result".forma(test_name))


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("HostLookup Test : {} : Unexpected certitude size of {} instead of {}"
                      .format(test_name, len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("HostLookup Test : {} : Unexpected certitude of {} instead of {}"
                      .format(test_name, certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    hostlookup_filter.clean_files()
    # ret = hostlookup_filter.valgrind_stop() or hostlookup_filter.valgrind_stop()
    # would erase upper ret if this function return True
    if not hostlookup_filter.valgrind_stop():
        ret = False

    return ret


"""
We give an untrusted host
"""
def unique_untrusted_host_test():
    return test(
        "unique_untrusted_host_test",
        [
         "good_host_1",
         "good_host_2",
         "good_host_3",
         ],
        [["very_bad_host"]],
        [0],1)

"""
We give a trusted host
"""
def unique_trusted_host_test():
    return test(
        "unique_trusted_host_test",
        [
         "good_host_1",
         "good_host_2",
         "good_host_3",
         ],
        [["good_host_1"]],[100],1)

"""
We give a trusted host
"""

def multiple_trusted_host_test():
    return test(
        "multiple_trusted_host_test",
        [
         "good_host_1",
         "good_host_2",
         "good_host_3",
         ],
        [
            ["good_host_3"],
            ["good_host_1"],
        ],
        [100,100],2)

"""
We give multiple untrusted hosts
"""
def multiple_untrusted_host_test():
    return test(
        "multiple_untrusted_host_test",
        [
         "good_host_1",
         "good_host_2",
         "good_host_3",
         ],
        [
            ["very_bad_host"],
            ["even_more_very_bad_host"],
            ["sneaky_host"],
        ],
        [0,0,0],3)

"""
We give multiple hosts
"""
def multiple_host_test():
    return test(
        "multiple_host_test",
        [
         "good_host_1",
         "good_host_2",
         "good_host_3",
         ],
        [
            ["very_bad_host"],
            ["even_more_very_bad_host"],
            ["good_host_2"],
            ["sneaky_host"],
            ["good_host_1"],
        ],
        [0,0,100,0,100],5)

"""
We give database file with differents endlines, 
blank lines, blank spaces and tabs
"""
def init_data_test():
    return test(
        "init_data_test",
        [
         "good_host_1\r",
         "\tgood_host_2",
         "  good_host_3  ",
         "  ",
         "",
         "good_host_4",
         ""
         ],
        [
            ["good_host_1"],
            ["good_host_2"],
            ["good_host_3"],
            ["good_host_4"],
            [""],
        ],
        [100,100,100,100,0],5)
