import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class YaraScan(Filter):
    def __init__(self):
        super().__init__(filter_name="yara_scan")
        self.rulefile = "/tmp/rules.yara"

    def configure(self):
        content = '{{\n' \
                  '"fastmode": false,\n' \
                  '"timout": 0,\n' \
                  '"rule_file_list": ["{rulefile}"]\n' \
                  '}}'.format(rulefile=self.rulefile)
        super(YaraScan, self).configure(content)

    def generate_rule_file(self, data):
        with open(self.rulefile, mode="w") as file:
            file.write(data)

    def clean_files(self):
        super(YaraScan, self).clean_files()

        try:
            os.remove(self.rulefile)
        except:
            pass


def run():
    tests = [
        non_matching_test,
        wrong_data_test,
        multiple_entries_one_match_test,
        multiple_entries_multiple_match_test,
        many_entries_matching_non_matching_wrong_test
    ]

    for i in tests:
        print_result("yara_scan: " + i.__name__, i())


"""
We give non-matching data
"""
def non_matching_test():
    ret = True

    # CONFIG
    yara_scan_filter = YaraScan()

    yara_scan_filter.configure()

    yara_scan_filter.generate_rule_file("""rule Darwin
    {
        strings:
            $s1 = { 64 61 72 77 69 6e }

        condition:
            all of them
    }""")

    # START FILTER
    yara_scan_filter.valgrind_start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=yara_scan_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            "Y291Y291IGMnZXN0IG1vaSwgbW91bW91IGxhIHJlaW5lIGRlcyBtb3VldHRlcyAh",
        ],
        filter_code="YARA_SCAN",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0]
    expected_certitudes_size = 1

    if certitudes is None:
        ret = False
        logging.error("yara_scan Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    # yara_scan_filter.clean_files()

    if not yara_scan_filter.valgrind_stop():
        ret = False

    return ret


"""
We give wrong data
"""
def wrong_data_test():
    ret = True

    # CONFIG
    yara_scan_filter = YaraScan()

    yara_scan_filter.configure()

    yara_scan_filter.generate_rule_file("""rule Darwin
    {
        strings:
            $s1 = { 64 61 72 77 69 6e }

        condition:
            all of them
    }""")

    # START FILTER
    yara_scan_filter.valgrind_start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=yara_scan_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            "un deux trois je m'en vais aux bois",
        ],
        filter_code="YARA_SCAN",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [101]
    expected_certitudes_size = 1

    if certitudes is None:
        ret = False
        logging.error("yara_scan Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    # yara_scan_filter.clean_files()

    if not yara_scan_filter.valgrind_stop():
        ret = False

    return ret

"""
We give non-matching (several) and matching (one) data
"""
def multiple_entries_one_match_test():
    ret = True

    # CONFIG
    yara_scan_filter = YaraScan()

    yara_scan_filter.configure()

    yara_scan_filter.generate_rule_file("""rule Darwin
    {
        strings:
            $s1 = { 64 61 72 77 69 6e }

        condition:
            all of them
    }""")

    # START FILTER
    yara_scan_filter.valgrind_start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=yara_scan_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            "Y291Y291IGMnZXN0IG1vaSwgbW91bW91IGxhIHJlaW5lIGRlcyBtb3VldHRlcyAh",
            "RGFyd2luIGFsbG93cyByZXZvbHV0aW9ucyB3aGVuIGl0J3MgbmVjZXNzYXJ5",
            "ZGlkIHlvdSBzYXkgZGFyd2luID8="
        ],
        filter_code="YARA_SCAN",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0, 0, 100]
    expected_certitudes_size = 3

    if certitudes is None:
        ret = False
        logging.error("yara_scan Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    yara_scan_filter.clean_files()

    if not yara_scan_filter.valgrind_stop():
        ret = False

    return ret



"""
We give different matching and non-matching data (several rules)
"""
def multiple_entries_multiple_match_test():
    ret = True

    # CONFIG
    yara_scan_filter = YaraScan()

    yara_scan_filter.configure()

    yara_scan_filter.generate_rule_file("""
    rule Darwin
    {
        strings:
            $s1 = { 64 61 72 77 69 6e }

        condition:
            all of them
    }

    rule test
    {
        strings:
            $s1 = "hippopotame"

        condition:
            all of them
    }""")

    # START FILTER
    yara_scan_filter.valgrind_start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=yara_scan_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            "Y291Y291IGMnZXN0IG1vaSwgbW91bW91IGxhIHJlaW5lIGRlcyBtb3VldHRlcyAh",
            "TWFpcywgdm91cyBzYXZleiwgbW9pIGplIG5lIGNyb2lzIHBhcwpxdSdpbCB5IGFpdCBkZSBib25uZSBvdSBkZSBtYXV2YWlzZSBzaXR1YXRpb24uCk1vaSwgc2kgamUgZGV2YWlzIHLDqXN1bWVyIG1hIHZpZSBhdWpvdXJkJ2h1aSBhdmVjIHZvdXMsCmplIGRpcmFpcyBxdWUgYydlc3QgZCdhYm9yZCBkZXMgcmVuY29udHJlcywKRGVzIGdlbnMgcXVpIG0nb250IHRlbmR1IGxhIG1haW4sCnBldXQtw6p0cmUgw6AgdW4gbW9tZW50IG/DuSBqZSBuZSBwb3V2YWlzIHBhcywgb8O5IGonw6l0YWlzIHNldWwgY2hleiBtb2kuCkV0IGMnZXN0IGFzc2V6IGN1cmlldXggZGUgc2UgZGlyZSBxdWUgbGVzIGhhc2FyZHMsCmxlcyByZW5jb250cmVzIGZvcmdlbnQgdW5lIGRlc3RpbsOpZS4uLgpQYXJjZSBxdWUgcXVhbmQgb24gYSBsZSBnb8O7dCBkZSBsYSBjaG9zZSwKcXVhbmQgb24gYSBsZSBnb8O7dCBkZSBsYSBjaG9zZSBiaWVuIGZhaXRlLApMZSBiZWF1IGdlc3RlLCBwYXJmb2lzIG9uIG5lIHRyb3V2ZSBwYXMgbCdpbnRlcmxvY3V0ZXVyIGVuIGZhY2UsCmplIGRpcmFpcywgbGUgbWlyb2lyIHF1aSB2b3VzIGFpZGUgw6AgYXZhbmNlci4KQWxvcnMgY2Ugbidlc3QgcGFzIG1vbiBjYXMsIGNvbW1lIGplIGxlIGRpc2FpcyBsw6AsCnB1aXNxdWUgbW9pIGF1IGNvbnRyYWlyZSwgaidhaSBwdSA7CkV0IGplIGRpcyBtZXJjaSDDoCBsYSB2aWUsIGplIGx1aSBkaXMgbWVyY2ksCmplIGNoYW50ZSBsYSB2aWUsIGplIGRhbnNlIGxhIHZpZS4uLiBKZSBuZSBzdWlzIHF1J2Ftb3VyIQpFdCBmaW5hbGVtZW50LCBxdWFuZCBiZWF1Y291cCBkZSBnZW5zIGF1am91cmQnaHVpIG1lIGRpc2VudCA6CmRhcndpbgoiTWFpcyBjb21tZW50IGZhaXMtdHUgcG91ciBhdm9pciBjZXR0ZSBodW1hbml0w6kgPyIsCkVoIGJpZW4gamUgbGV1ciByw6lwb25kcyB0csOocyBzaW1wbGVtZW50LApqZSBsZXVyIGRpcyBxdWUgYydlc3QgY2UgZ2/Du3QgZGUgbCdhbW91ciwKQ2UgZ2/Du3QgZG9uYyBxdWkgbSdhIHBvdXNzw6kgYXVqb3VyZCdodWkKw6AgZW50cmVwcmVuZHJlIHVuZSBjb25zdHJ1Y3Rpb24gbcOpY2FuaXF1ZSwKTWFpcyBkZW1haW4sIHF1aSBzYWl0LCBwZXV0LcOqdHJlIHNpbXBsZW1lbnQKw6AgbWUgbWV0dHJlIGF1IHNlcnZpY2UgZGUgbGEgY29tbXVuYXV0w6ksCsOgIGZhaXJlIGxlIGRvbiwgbGUgZG9uIGRlIHNvaS4uLg==",
            "aGlwcG9kYXJ3aW5wb3RhbWU=",
            "ZGFyd2luCkxvcmVtIGlwc3VtIGRvbG9yIHpNdCBhbWV0LApoaXBwb3BvdGFtZQpjb25zZWN0ZXR1ciBhZGlwaXNjaW5nIGVsaXQu",
            "SGlwcG9wb3RhbWUu",
            "Y29tYmllbiBkJ2hpcHBvcG90YW1lcyA/"
        ],
        filter_code="YARA_SCAN",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0, 100, 100, 100, 0, 100]
    expected_certitudes_size = 6

    if certitudes is None:
        ret = False
        logging.error("yara_scan Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    yara_scan_filter.clean_files()

    if not yara_scan_filter.valgrind_stop():
        ret = False

    return ret



"""
We give different matching, non-matching and wrong data (several rules)
"""
def many_entries_matching_non_matching_wrong_test():
    ret = True

    # CONFIG
    yara_scan_filter = YaraScan()

    yara_scan_filter.configure()

    yara_scan_filter.generate_rule_file("""
    rule Darwin
    {
        strings:
            $s1 = { 64 61 72 77 69 6e }

        condition:
            all of them
    }

    rule test
    {
        strings:
            $s1 = "hippopotame"

        condition:
            all of them
    }""")

    # START FILTER
    yara_scan_filter.valgrind_start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=yara_scan_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            "Y291Y291IGMnZXN0IG1vaSwgbW91bW91IGxhIHJlaW5lIGRlcyBtb3VldHRlcyAh",
            "UGFscGF0aW5lOiBEaWQgeW91IGV2ZXIgaGVhciB0aGUgdHJhZ2VkeSBvZiBEYXJ0aCBQbGFndWVpcyBUaGUgV2lzZT8KCkFuYWtpbjogTm8/CgpQYWxwYXRpbmU6IEkgdGhvdWdodCBub3QuIEl0J3Mgbm90IGEgc3RvcnkgdGhlIEplZGkgd291bGQgdGVsbCB5b3UuIEl0J3MgYSBTaXRoIGxlZ2VuZC4gRGFydGggUGxhZ3VlaXMgd2FzIGEgRGFyayBMb3JkIG9mIHRoZSBTaXRoLCBzbyBkYXJ3aW5mdWwgYW5kIHNvIHdpc2UgaGUgY291bGQgdXNlIHRoZSBGb3JjZSB0byBpbmZsdWVuY2UgdGhlIG1pZGljaGxvcmlhbnMgdG8gY3JlYXRlIGxpZmXigKYgSGUgaGFkIHN1Y2ggYSBrbm93bGVkZ2Ugb2YgdGhlIGRhcmsgc2lkZSwgaGUgY291bGQgZXZlbiBrZWVwIHRoZSBvbmVzIGhlIGNhcmVkIGFib3V0IGZyb20gZHlpbmcuCgpBbmFraW46IEhlIGNvdWxkIGFjdHVhbGx5IHNhdmUgcGVvcGxlIGZyb20gZGVhdGg/CgpQYWxwYXRpbmU6IFRoZSBkYXJrIHNpZGUgb2YgdGhlIEZvcmNlIGlzIGEgcGF0aHdheSB0byBtYW55IGFiaWxpdGllcyBzb21lIGNvbnNpZGVyIHRvIGJlIHVubmF0dXJhbC4KCkFuYWtpbjogV2hhdCBoYXBwZW5lZCB0byBoaW0/CgpQYWxwYXRpbmU6IEhlIGJlY2FtZSBzbyBkYXJ3aW5mdWzigKYgdGhlIG9ubHkgdGhpbmcgaGUgd2FzIGFmcmFpZCBvZiB3YXMgbG9zaW5nIGhpcyBkYXJ3aW4sIHdoaWNoIGV2ZW50dWFsbHksIG9mIGNvdXJzZSwgaGUgZGlkLiBVbmZvcnR1bmF0ZWx5LCBoZSB0YXVnaHQgaGlzIGFwcHJlbnRpY2UgZXZlcnl0aGluZyBoZSBrbmV3LCB0aGVuIGhpcyBhcHByZW50aWNlIGtpbGxlZCBoaW0gaW4gaGlzIHNsZWVwLiBJcm9uaWMuIEhlIGNvdWxkIHNhdmUgb3RoZXJzIGZyb20gZGVhdGgsIGJ1dCBub3QgaGltc2VsZi4KCkFuYWtpbjogSXMgaXQgcG9zc2libGUgdG8gbGVhcm4gdGhpcyBkYXJ3aW4/CgpQYWxwYXRpbmU6IE5vdCBmcm9tIGEgSmVkaS4=",
            "sloubi un, sloubi deux, sloubi trois...",
            "aGlwcG9kYXJ3aW5wb3RhbWU=",
            "ZGFyd2luCkxvcmVtIGlwc3VtIGRvbG9yIHpNdCBhbWV0LApoaXBwb3BvdGFtZQpjb25zZWN0ZXR1ciBhZGlwaXNjaW5nIGVsaXQu",
            "SGlwcG9wb3RhbWUu= =",
            "Y29tYmllbiBkJ2hpcHBvcG90YW1lcyA/"
        ],
        filter_code="YARA_SCAN",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0, 100, 101, 100, 100, 101, 100]
    expected_certitudes_size = 7

    if certitudes is None:
        ret = False
        logging.error("yara_scan Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    yara_scan_filter.clean_files()

    if not yara_scan_filter.valgrind_stop():
        ret = False

    return ret