import os
import logging

from darwin import DarwinApi
from tools.filter import Filter
from tools.output import print_result
from tools.logger import CustomAdapter

class HostLookup(Filter):
    def __init__(self, logger):
        super().__init__(filter_name="hostlookup", logger=logger)
        self.database = "/tmp/database.txt"

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
    global logger
    glogger = logging.getLogger("HOSTLOOKUP")

    tests = [
        unique_untrusted_host_test,
        unique_trusted_host_test,
        multiple_trusted_host_test,
        multiple_untrusted_host_test,
        multiple_host_test,
        init_data_test
    ]

    for i in tests:
        logger = CustomAdapter(glogger, {'test_name': i.__name__})
        print_result("hostlookup: " + i.__name__, i)


def test(test_name, init_data, data, expected_certitudes, expected_certitudes_size):
    ret = True

    # CONFIG
    hostlookup_filter = HostLookup(logger=logger)
    # All the trusted hosts
    hostlookup_filter.init_data(init_data)
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return False

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
        logger.error("No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logger.error("Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logger.error("Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

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
