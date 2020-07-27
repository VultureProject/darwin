import json
import logging
import os

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class Yara(Filter):
    def __init__(self):
        super().__init__(filter_name="yara")
        self.rulefiles = []
        self.rulefiles.append("/tmp/rules.yara")

    def configure(self):
        content = '{{\n' \
                  '"fastmode": true,\n' \
                  '"timout": 0,\n' \
                  '"rule_file_list": {rulefiles}\n' \
                  '}}'.format(rulefiles=json.dumps(self.rulefiles))
        super(Yara, self).configure(content)

    def generate_rule_file(self, data, rulefile=None):
        if rulefile is not None:
            self.rulefiles.append(rulefile)
        with open(self.rulefiles[-1], mode="w") as file:
            file.write(data)

    def clean_files(self):
        super(Yara, self).clean_files()

        try:
            for file in self.rulefiles:
                os.remove(file)
        except:
            pass


def run():
    tests = [
        no_rule_files,
        one_valid_rule_file,
        several_valid_rule_files,
        several_valid_rule_files_with_duplicate,
        hex_input_several_rules_no_match,
        hex_input_several_rules_one_match,
        two_hex_inputs_several_rules_two_matches,
        hex_input_several_rules_wrong_data,
        base64_input_several_rules_no_match,
        base64_input_several_rules_one_match,
        two_base64_inputs_several_rules_two_matches,
        base64_input_several_rules_wrong_data,
        plain_input_several_rules_no_match,
        plain_input_several_rules_one_match,
        two_plain_inputs_several_rules_two_matches,
        many_inputs_several_rules_different_results,
    ]

    for i in tests:
        print_result("yara_scan: " + i.__name__, i)


"""
No rule file in configuration
"""
def no_rule_files():
    yara_filter = Yara()
    yara_filter.configure()

    if yara_filter.valgrind_start():
        logging.error("no_rule_files: filter starter without any rule file")
        yara_filter.valgrind_stop()
        return False

    return True

"""
One valid rule file in configuration
"""
def one_valid_rule_file():
    yara_filter = Yara()
    yara_filter.generate_rule_file("""rule Darwin
    {
        strings:
            $s1 = { 44 61 72 77 69 6e }

        condition:
            all of them
    }""")
    yara_filter.configure()


    if not yara_filter.valgrind_start():
        logging.error("one_valid_rule_file: filter didn't start")
        return False

    if not yara_filter.valgrind_stop():
        logging.error("one_valid_rule_file: filter didn't stop properly")
        return False

    return True

"""
Several valid rule files in configuration
"""
def several_valid_rule_files():
    yara_filter = Yara()
    yara_filter.generate_rule_file("""rule Darwin
    {
        strings:
            $s1 = { 44 61 72 77 69 6e }

        condition:
            all of them
    }""")
    yara_filter.generate_rule_file("""rule niwraD
    {
        strings:
            $s1 = { 6e 69 77 72 61 44 }

        condition:
            all of them
    }""", rulefile="/tmp/rules2.yara")
    yara_filter.configure()


    if not yara_filter.valgrind_start():
        logging.error("one_valid_rule_file: filter didn't start")
        return False

    if not yara_filter.valgrind_stop():
        logging.error("one_valid_rule_file: filter didn't stop properly")
        return False

    return True


"""
Several valid rule files in configuration, but with a duplicate rule
"""
def several_valid_rule_files_with_duplicate():
    yara_filter = Yara()
    yara_filter.generate_rule_file("""rule Darwin
    {
        strings:
            $s1 = { 44 61 72 77 69 6e }

        condition:
            all of them
    }""")
    yara_filter.generate_rule_file("""rule niwraD
    {
        strings:
            $s1 = { 6e 69 77 72 61 44 }

        condition:
            all of them
    }
    rule Darwin
    {
        strings:
            $s1 = { 44 61 72 77 69 6e }

        condition:
            all of them
    }""", rulefile="/tmp/rules2.yara")
    yara_filter.configure()


    if yara_filter.valgrind_start():
        logging.error("one_valid_rule_file: filter started with a duplicate rule")
        yara_filter.valgrind_stop()
        return False

    return True


"""
helper function for tests related to matching
"""
def input_with_several_rules(data, expected_certitudes=[0]):
    ret = True
    yara_filter = Yara()
    yara_filter.generate_rule_file("""rule niwraD
    {
        strings:
            $s1 = { 6e 69 77 72 61 44 }

        condition:
            all of them
    }
    rule Darwin
    {
        strings:
            $s1 = { 44 61 72 77 69 6e }

        condition:
            all of them
    }""")
    yara_filter.configure()

    if not yara_filter.valgrind_start():
        logging.error("one_valid_rule_file: filter didn't start")
        return False

        # SEND TEST
    darwin_api = DarwinApi(socket_path=yara_filter.socket,
                           socket_type="unix")



    results = darwin_api.bulk_call(
        data,
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')

    if certitudes is None:
        ret = False
        logging.error("yara_scan Test : No certitude list found in result")


    if len(certitudes) != len(expected_certitudes):
        ret = False
        logging.error("yara_scan Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), len(expected_certitudes)))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("yara_scan Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    if not yara_filter.valgrind_stop():
        logging.error("one_valid_rule_file: filter didn't stop properly")
        return False

    return ret






"""
A rule file with several rules, a base 16 encoded input, no match
"""
def hex_input_several_rules_no_match():
    return input_with_several_rules(data=[["54686973207265706f72742c206279206974732076657279206c656e6774682c20646566656e647320697473656c6620616761696e737420746865207269736b206f66206265696e6720726561642e0a57696e73746f6e20436875726368696c6c0a", "hex"]])


"""
A rule file with several rules, a base 16 encoded input, a match
"""
def hex_input_several_rules_one_match():
    return input_with_several_rules(data=[["546f20646f20612067726561742044617277696e20646f2061206c6974746c652077726f6e672e0a286e6f74292057696c6c69616d205368616b65737065617265", "hex"]], expected_certitudes=[100])

"""
A rule file with several rules, 2 base 16 encoded inputs, 2 matches
"""
def two_hex_inputs_several_rules_two_matches():
    return input_with_several_rules(data=[["546f20646f20612067726561742044617277696e20646f2061206c6974746c652077726f6e672e0a286e6f74292057696c6c69616d205368616b65737065617265", "hex"],
                                    ["6e69777261442c206e69777261442c206e6977726144", "hex"]], expected_certitudes=[100, 100])

"""
A rule file with several rules, a wrong base 16 encoded input, no match
"""
def hex_input_several_rules_wrong_data():
    return input_with_several_rules(data=[["54686973207265706f72742c206279206974732076657279206c656e6774682c20646566656e647320697473656c6620616761696e737420746865207269736b206f66206265696e6720726561642e0a57696e73746f6e20436875726368696c6czz", "hex"]], expected_certitudes=[101])




"""
A rule file with several rules, a base 64 encoded input, no match
"""
def base64_input_several_rules_no_match():
    return input_with_several_rules(data=[["SSBkbyBub3QgZmVhciBjb21wdXRlcnMuIEkgZmVhciB0aGUgbGFjayBvZiB0aGVtLgpJc2FhYyBBc2ltb3Y=", "base64"]])


"""
A rule file with several rules, a base 64 encoded input, a match
"""
def base64_input_several_rules_one_match():
    return input_with_several_rules(data=[["QXMgRGFyd2luIGhpbXNlbGYgd2FzIGF0IHBhaW5zIHRvIHBvaW50IG91dCwgbmF0dXJhbCBzZWxlY3Rpb24gaXMgYWxsIGFib3V0IGRpZmZlcmVudGlhbCBzdXJ2aXZhbCB3aXRoaW4gc3BlY2llcywgbm90IGJldHdlZW4gdGhlbS4KUmljaGFyZCBEYXdraW5z", "base64"]], expected_certitudes=[100])

"""
A rule file with several rules, 2 base 64 encoded inputs, 2 matches
"""
def two_base64_inputs_several_rules_two_matches():
    return input_with_several_rules(data=[["QXMgRGFyd2luIGhpbXNlbGYgd2FzIGF0IHBhaW5zIHRvIHBvaW50IG91dCwgbmF0dXJhbCBzZWxlY3Rpb24gaXMgYWxsIGFib3V0IGRpZmZlcmVudGlhbCBzdXJ2aXZhbCB3aXRoaW4gc3BlY2llcywgbm90IGJldHdlZW4gdGhlbS4KUmljaGFyZCBEYXdraW5z", "base64"],
                                    ["bml3cmFELCBuaXdyYUQgbml3cmFELi4u", "base64"]], expected_certitudes=[100, 100])

"""
A rule file with several rules, a wrong base 64 encoded input, no match
"""
def base64_input_several_rules_wrong_data():
    return input_with_several_rules(data=[["TXkgdGhlb3J5IG9mIGV2b2x1dGlvbiBpcyB0aGF0IERhcndpbiB3YXMgYWRvcHRlZC4KU3RldmVuIFdyaWdodAo", "base64"]], expected_certitudes=[101])





"""
A rule file with several rules, a plain input, no match
"""
def plain_input_several_rules_no_match():
    return input_with_several_rules(data=[["This input won't match"]])


"""
A rule file with several rules, a plain input, a match
"""
def plain_input_several_rules_one_match():
    return input_with_several_rules(data=[["Darwin"]], expected_certitudes=[100])

"""
A rule file with several rules, 2 plain inputs, 2 matches
"""
def two_plain_inputs_several_rules_two_matches():
    return input_with_several_rules(data=[["Darwin"],
                                    ["niwraD"]], expected_certitudes=[100, 100])



"""
A rule file with several rules, several inputs with different encodings, some matches
"""
def many_inputs_several_rules_different_results():
    return input_with_several_rules(data=[  ["7468697320776f6e2774206d61746368", "hex"],
                                            ["44617277696e2069732074686520616e73776572", "hex"],
                                            ["00112233445566778899aabbccddeeffoups", "hex"],
                                            ["SXQncyBhIHRyYXAgIQ= =", "base64"],
                                            ["VGhpcyB3b24ndCBtYXRjaCBlaXRoZXI=", "base64"],
                                            ["bml3cmFEIGlzIGp1c3QgdGhlIHJlZmxleGlvbiBvZiBEYXJ3aW4u", "base64"],
                                            [""],
                                            ["this is justDarwin matching"],
                                            ], expected_certitudes=[0, 100, 101, 101, 0, 100, 0, 100])

