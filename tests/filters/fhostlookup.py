import logging
import os
import json

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class HostLookup(Filter):
    def __init__(self):
        super().__init__(filter_name="hostlookup", thresold=80)
        self.database = "/tmp/database.txt".format(self.filter_name)

    def configure(self):
        content = '{{\n' \
                  '"database": "{database}"\n' \
                  '}}'.format(database=self.database)
        super(HostLookup, self).configure(content)

    def init_data(self, data):
        with open(self.database, mode='w') as file:
            file.write(json.dumps(data))

    def clean_files(self):
        super(HostLookup, self).clean_files()

        try:
            os.remove(self.init_data_path)
        except:
            pass


def run():
    tests = [
        db_no_feed_name,
        db_wrong_feed_name_type,
        db_no_data_field,
        db_wrong_data_type,
        db_empty_data,
        db_no_entry_field,
        db_no_usable_entry_field,
        db_worng_entry_type,
        exec_one_good,
        exec_one_bad,
        exec_one_bad_no_score,
        exec_one_bad_score_lt_0,
        exec_one_bad_score_gt_100,
        exec_one_bad_wrong_score_type,
        exec_multiple_good,
        exec_multiple_bad,
        exec_one_bad_multiple_good,
        exec_multiple_bad_multiple_good
    ]

    for i in tests:
        print_result("hostlookup: " + i.__name__, i)


def test(test_name, init_data, data, expected_certitudes):
    ret = True

    # CONFIG
    hostlookup_filter = HostLookup()
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
        logging.error("Hostlookup Test : {} : No certitude list found in result".format(test_name))


    if len(certitudes) != len(expected_certitudes):
        ret = False
        logging.error("HostLookup Test : {} : Unexpected certitude size of {} instead of {}"
                      .format(test_name, len(certitudes), len(expected_certitudes)))

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


def db_no_feed_name():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "data": [
            {"entry": "bad_host_1", "score": 84},
            {"entry": "bad_host_2", "score": 42},
            {"entry": "bad_host_3", "score": 100},
        ]
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def db_wrong_feed_name_type():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": 42,
        "data": [
            {"entry": "bad_host_1", "score": 84},
            {"entry": "bad_host_2", "score": 42},
            {"entry": "bad_host_3", "score": 100},
        ]
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def db_no_data_field():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": "test-feed",
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def db_wrong_data_type():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": "test-feed",
        "data": "It's Supposed To Be Automatic But Actually You Have To Press This Button. John Brunner, Stand on Zanzibar."
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def db_empty_data():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": "test-feed",
        "data": []
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def db_no_entry_field():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": "test-feed",
        "data": [
            {"score": 84},
            {"entry": "bad_host_2", "score": 42},
            {"entry": "bad_host_3", "score": 100},
        ]
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return False
    return True


def db_no_usable_entry_field():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": "test-feed",
        "data": [
            {"score": 84},
            {"score": 42},
            {"score": 100},
        ]
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def db_worng_entry_type():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "feed_name": "test-feed",
        "data": [
            {"entry": 42, "score": 84},
            {"entry": "bad_host_2", "score": 42},
            {"entry": "bad_host_3", "score": 100},
        ]
    })
    hostlookup_filter.configure()

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return False
    return True


def exec_one_good():
    return test(
        "exec_one_good",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 100},
            ]
        },
        [["good_host"]],
        [0]
    )


def exec_one_bad():
    return test(
        "exec_one_bad",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 100},
            ]
        },
        [["bad_host_2"]],
        [42]
    )


def exec_one_bad_no_score():
    return test(
        "exec_one_bad_no_score",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3"},
            ]
        },
        [["bad_host_3"]],
        [100]
    )


def exec_one_bad_score_lt_0():
    return test(
        "exec_one_bad_score_lt_0",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": -42},
            ]
        },
        [["bad_host_3"]],
        [100]
    )


def exec_one_bad_score_gt_100():
    return test(
        "exec_one_bad_score_gt_100",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 1337},
            ]
        },
        [["bad_host_3"]],
        [100]
    )


def exec_one_bad_wrong_score_type():
    return test(
        "exec_one_bad_wrong_score_type",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": "When done well, software is invisible. Bjarne Stroustrup."},
            ]
        },
        [["bad_host_3"]],
        [100]
    )


def exec_multiple_good():
    return test(
        "exec_multiple_good",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 100},
            ]
        },
        [
            ["good_host_1"],
            ["good_host_2"],
            ["good_host_3"],
            ["good_host_4"],
        ],
        [0, 0, 0, 0]
    )


def exec_multiple_bad():
    return test(
        "exec_multiple_bad",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 100},
            ]
        },
        [
            ["bad_host_2"],
            ["bad_host_1"],
            ["bad_host_2"],
            ["bad_host_3"],
        ],
        [42, 84, 42, 100]
    )


def exec_one_bad_multiple_good():
    return test(
        "exec_one_bad_multiple_good",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 100},
            ]
        },
        [
            ["good_host_1"],
            ["good_host_2"],
            ["bad_host_2"],
            ["good_host_3"],
        ],
        [0, 0, 42, 0]
    )


def exec_multiple_bad_multiple_good():
    return test(
        "exec_multiple_bad_multiple_good",
        {
            "feed_name": "test-feed",
            "data": [
                {"entry": "bad_host_1", "score": 84},
                {"entry": "bad_host_2", "score": 42},
                {"entry": "bad_host_3", "score": 100},
            ]
        },
        [
            ["good_host_1"],
            ["bad_host_2"],
            ["good_host_2"],
            ["bad_host_1"],
            ["good_host_3"],
        ],
        [0, 42, 0, 84, 0]
    )
