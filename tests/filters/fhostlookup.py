import logging
import os
import json

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class HostLookup(Filter):
    def __init__(self):
        super().__init__(filter_name="hostlookup", threshold=80)
        self.database = "/tmp/database.txt".format(self.filter_name)

    def configure(self, db_type="json"):
        if not db_type:
            content = '{{\n' \
                  '"database": "{database}"\n' \
                  '}}'.format(database=self.database)
        else:
            content = '{{\n' \
                      '"database": "{database}",\n' \
                      '"db_type": "{db_type}"\n' \
                      '}}'.format(database=self.database, db_type=db_type)
        super(HostLookup, self).configure(content)

    def init_data(self, data):
        with open(self.database, mode='w') as file:
            if type(data) == dict:
                file.write(json.dumps(data))
            else:
                file.write(data)

    def clean_files(self):
        super(HostLookup, self).clean_files()

        try:
            os.remove(self.init_data_path)
        except:
            pass


def run():
    tests = [
        bad_db_type,
        json_db_no_feed_name,
        json_db_wrong_feed_name_type,
        json_db_no_data_field,
        json_db_wrong_data_type,
        json_db_empty_data,
        json_db_no_entry_field,
        json_db_no_usable_entry_field,
        json_db_wrong_entry_type,
        rsyslog_db_no_table,
        rsyslog_db_empty_table,
        rsyslog_db_no_value,
        rsyslog_db_no_index,
        rsyslog_db_wrong_index_type,
        rsyslog_db_wrong_value_type,
        rsyslog_db_no_viable_entry,
        exec_one_good,
        exec_one_bad,
        exec_one_bad_no_score,
        exec_one_bad_score_lt_0,
        exec_one_bad_score_gt_100,
        exec_one_bad_wrong_score_type,
        exec_multiple_good,
        exec_multiple_bad,
        exec_one_bad_multiple_good,
        exec_multiple_bad_multiple_good,
        exec_rsyslog_int_multiple_bad_multiple_good,
        exec_rsyslog_str_multiple_bad_multiple_good,
        exec_rsyslog_mix_multiple_bad_multiple_good,
        exec_text_multiple_bad_multiple_good,
        exec_no_db_type_multiple_bad_multiple_good,
    ]

    for i in tests:
        print_result("hostlookup: " + i.__name__, i)


def test(test_name, init_data, data, expected_certitudes, db_type="json"):
    ret = True

    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data(init_data)
    hostlookup_filter.configure(db_type=db_type)

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


def bad_db_type():
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
    hostlookup_filter.configure(db_type="foobar")

    # START FILTER
    if not hostlookup_filter.valgrind_start():
        return True
    return False


def json_db_no_feed_name():
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


def json_db_wrong_feed_name_type():
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


def json_db_no_data_field():
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


def json_db_wrong_data_type():
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


def json_db_empty_data():
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


def json_db_no_entry_field():
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


def json_db_no_usable_entry_field():
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


def json_db_wrong_entry_type():
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


def rsyslog_db_no_table():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
        return False
    return True


def rsyslog_db_empty_table():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
        "table": []
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
        return False
    return True


def rsyslog_db_no_value():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
        "table": [
            {"index": "bad_host_1"},
        ]
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
        return False
    return True


def rsyslog_db_no_index():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
        "table": [
            {"value": 84},
        ]
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
        return False
    return True


def rsyslog_db_wrong_index_type():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
        "table": [
            {"index": 42, "value": 84},
        ]
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
        return False
    return True


def rsyslog_db_wrong_value_type():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
        "table": [
            {"index": "bad_host_1", "value": []},
        ]
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
        return False
    return True


def rsyslog_db_no_viable_entry():
    # CONFIG
    hostlookup_filter = HostLookup()
    # All the trusted hosts
    hostlookup_filter.init_data({
        "version": 1,
        "nomatch": "unk",
        "type": "string",
        "table": [
            {"index": 42, "value": 84},
            {"index": "bad_host_2", "value": []},
            {"index": "bad_host_3", "value": 42.42},
        ]
    })
    hostlookup_filter.configure(db_type="rsyslog")

    # START FILTER
    if hostlookup_filter.valgrind_start():
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


def exec_rsyslog_int_multiple_bad_multiple_good():
    return test(
        "exec_multiple_bad_multiple_good",
        {
            "version": 1,
            "nomatch": "unk",
            "type": "string",
            "table": [
                {"index": "bad_host_1", "value": 84},
                {"index": "bad_host_2", "value": 42},
                {"index": "bad_host_3", "value": 100},
            ]
        },
        [
            ["good_host_1"],
            ["bad_host_2"],
            ["good_host_2"],
            ["bad_host_1"],
            ["good_host_3"],
        ],
        [0, 42, 0, 84, 0],
        db_type="rsyslog"
    )


def exec_rsyslog_str_multiple_bad_multiple_good():
    return test(
        "exec_multiple_bad_multiple_good",
        {
            "version": 1,
            "nomatch": "unk",
            "type": "string",
            "table": [
                {"index": "bad_host_1", "value": "host_1"},
                {"index": "bad_host_2", "value": "host_2"},
                {"index": "bad_host_3", "value": "host_3"},
            ]
        },
        [
            ["good_host_1"],
            ["bad_host_2"],
            ["good_host_2"],
            ["bad_host_1"],
            ["good_host_3"],
        ],
        [0, 100, 0, 100, 0],
        db_type="rsyslog"
    )


def exec_rsyslog_mix_multiple_bad_multiple_good():
    return test(
        "exec_multiple_bad_multiple_good",
        {
            "version": 1,
            "nomatch": "unk",
            "type": "string",
            "table": [
                {"index": "bad_host_1", "value": 84},
                {"index": "bad_host_2", "value": "host_2"},
                {"index": "bad_host_3", "value": 100},
            ]
        },
        [
            ["good_host_1"],
            ["bad_host_2"],
            ["good_host_2"],
            ["bad_host_1"],
            ["good_host_3"],
        ],
        [0, 100, 0, 84, 0],
        db_type="rsyslog"
    )


def exec_text_multiple_bad_multiple_good():
    return test(
        "exec_multiple_bad_multiple_good",
        """bad_host_1
bad_host_2
bad_host_3"""
        ,
        [
            ["good_host_1"],
            ["bad_host_2"],
            ["good_host_2"],
            ["bad_host_1"],
            ["good_host_3"],
        ],
        [0, 100, 0, 100, 0],
        db_type="text"
    )


def exec_no_db_type_multiple_bad_multiple_good():
    return test(
        "exec_multiple_bad_multiple_good",
        """bad_host_1
bad_host_2
bad_host_3"""
        ,
        [
            ["good_host_1"],
            ["bad_host_2"],
            ["good_host_2"],
            ["bad_host_1"],
            ["good_host_3"],
        ],
        [0, 100, 0, 100, 0],
        db_type=None
    )