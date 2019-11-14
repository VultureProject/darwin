import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class Anomaly(Filter):
    def __init__(self):
        super().__init__(filter_name="anomaly")

    def configure(self):
        content = "{}"
        super(Anomaly, self).configure(content)


def run():
    tests = [
        not_enough_data_test,
        too_short_line_data_test,
        no_string_line_data_test,
        no_int_line_data_test,
        line_not_list_data_test,
        not_list_data_test,
        one_set_data_test,
        multiple_set_data_test,
    ]

    for i in tests:
        print_result("anomaly: " + i.__name__, i)


def test(test_name, data, expected_certitudes, expected_certitudes_size):
    ret = True

    # CONFIG
    anomaly_filter = Anomaly()
    # All the trusted hosts
    anomaly_filter.configure()

    # START FILTER
    if not anomaly_filter.start():
        return False

    # SEND TEST
    darwin_api = DarwinApi(socket_path=anomaly_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        data,
        filter_code="ANOMALY",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')

    if certitudes is None:
        ret = False
        logging.error("Anomaly Test : {} : No certitude list found in result".format(test_name))


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Anomaly Test : {} : Unexpected certitude size of {} instead of {}"
                      .format(test_name, len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Anomaly Test : {} : Unexpected certitude of {} instead of {}"
                      .format(test_name, certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    anomaly_filter.clean_files()
    if not anomaly_filter.stop():
        ret = False

    return ret

def not_enough_data_test():
    return test(
        "not_enough_data_test",
        [[["42.42.42.42",42,42,42,42,42]]],
        [101],1)

def one_set_data_test():
    return test(
        "one_set_data_test",
        [
            [
                ["20.245.247.107",16,29,100,99,98],
                ["219.218.162.234",90,27,40,96,82],
                ["186.128.98.80",84,11,20,87,59],
                ["168.84.159.133",44,67,84,78,35],
                ["187.254.171.124",57,57,51,8,94],
                ["12.114.194.153",15,24,7,24,74],
                ["91.204.62.178",77,51,48,12,31],
                ["82.92.146.207",101,93,55,94,53],
                ["158.87.120.73",20,34,44,95,74],
                ["245.196.164.104",6,35,62,41,89],
                ["106.175.196.38",29,31,96,18,87],
            ],
        ],
        [0],1)

def multiple_set_data_test():
    return test(
        "multiple_set_data_test",
        [
            [
                ["20.245.247.107",16,29,100,99,98],
                ["219.218.162.234",90,27,40,96,82],
                ["186.128.98.80",84,11,20,87,59],
                ["168.84.159.133",44,67,84,78,35],
                ["187.254.171.124",57,57,51,8,94],
                ["12.114.194.153",15,24,7,24,74],
                ["91.204.62.178",77,51,48,12,31],
                ["82.92.146.207",101,93,55,94,53],
                ["158.87.120.73",20,34,44,95,74],
                ["245.196.164.104",6,35,62,41,89],
                ["106.175.196.38",29,31,96,18,87],
            ],
            [
                ["140.217.116.146",74,61,26,2,79],
                ["39.67.10.76",93,34,47,86,97],
                ["53.101.255.67",36,36,4,45,60],
                ["110.214.19.254",22,75,62,26,75],
                ["186.9.165.125",34,36,29,60,46],
                ["57.220.242.136",23,88,15,99,95],
                ["223.118.198.40",86,22,30,39,30],
                ["59.11.64.89",7,16,38,37,73],
                ["17.159.187.225",70,69,57,50,81],
                ["192.82.162.41",61,47,76,51,31],
                ["73.62.247.250",64,70,37,84,7],
                ["56.42.45.251",28,75,14,88,81],
                ["110.75.226.138",75,52,22,39,87],
            ],
        ],
        [0,0],2)

def too_short_line_data_test():
    return test(
        "too_short_line_data_test",
        [
            [
                ["42.42.42.42",1,42,42,42,42],
                ["42.42.42.42",2,42,42,42,42],
                ["42.42.42.42",3,42,42,42,42],
                ["42.42.42.42",4,42,42,42,42],
                ["42.42.42.42",5,42,42,42,42],
                ["42.42.42.42",6,42,42,42,42],
                ["42.42.42.42",7,42,42,42,42],
                ["42.42.42.42",8,42,42,42,42],
                ["42.42.42.42",9,42,42,42,42],
                ["42.42.42.42",10,42,42,42]
            ],
        ],
        [101],1)

def no_string_line_data_test():
    return test(
        "no_string_line_data_test",
        [
            [
                ["42.42.42.42",1,42,42,42,42],
                ["42.42.42.42",2,42,42,42,42],
                ["42.42.42.42",3,42,42,42,42],
                ["42.42.42.42",4,42,42,42,42],
                ["42.42.42.42",5,42,42,42,42],
                ["42.42.42.42",6,42,42,42,42],
                ["42.42.42.42",7,42,42,42,42],
                ["42.42.42.42",8,42,42,42,42],
                ["42.42.42.42",9,42,42,42,42],
                [42,10,42,42,42,42]
            ],
        ],
        [101],1)

def no_int_line_data_test():
    return test(
        "no_int_line_data_test",
        [
            [
                ["42.42.42.42",1,42,42,42,42],
                ["42.42.42.42",2,42,42,42,42],
                ["42.42.42.42",3,42,42,42,42],
                ["42.42.42.42",4,42,42,42,42],
                ["42.42.42.42",5,42,42,42,42],
                ["42.42.42.42",6,42,42,42,42],
                ["42.42.42.42",7,42,42,42,42],
                ["42.42.42.42",8,42,42,42,42],
                ["42.42.42.42",9,42,42,42,42],
                ["42.42.42.42","10","42","42","42","42"],
            ],
        ],
        [101],1)

def not_list_data_test():
    return test(
        "not_list_data_test",
        [
            ["42.42.42.42",1,42,42,42,42],
            ["42.42.42.42",2,42,42,42,42],
            ["42.42.42.42",3,42,42,42,42],
            ["42.42.42.42",4,42,42,42,42],
            ["42.42.42.42",5,42,42,42,42],
            ["42.42.42.42",6,42,42,42,42],
            ["42.42.42.42",7,42,42,42,42],
            "42.42.42.42",
            ["42.42.42.42",8,42,42,42,42],
            ["42.42.42.42",9,42,42,42,42],
        ],
        [101,101,101,101,101,101,101,101,101,101],10)

def line_not_list_data_test():
    return test(
        "line_not_list_data_test",
        [
            [
                ["42.42.42.42",1,42,42,42,42],
                ["42.42.42.42",2,42,42,42,42],
                ["42.42.42.42",3,42,42,42,42],
                ["42.42.42.42",4,42,42,42,42],
                ["42.42.42.42",5,42,42,42,42],
                ["42.42.42.42",6,42,42,42,42],
                ["42.42.42.42",7,42,42,42,42],
                "42.42.42.42",
                ["42.42.42.42",8,42,42,42,42],
                ["42.42.42.42",9,42,42,42,42],
            ],
        ],
        [101],1)