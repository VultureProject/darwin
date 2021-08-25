import json
import logging
import os
import random
import numpy

from tools.filter import Filter
from tools.output import print_result

def run():
    tests = [
        random_values,
        wrong_data_empty,
        wrong_data_string
    ]

    for i in tests:
        print_result("N Sigma: " + i.__name__, i)


class NSigmaFilter(Filter):
    n = 3
    log_filename = "/tmp/nsigma.log"
    def __init__(self, n=3):
        super().__init__(filter_name="nsigma")
        self.n = n
    
    def configure(self):
        super().configure('{{"n_sigma":{}, "log_file_path":"{}"}}'.format(self.n, self.log_filename))

    def clean_files(self):
        super().clean_files()

        try:
            os.remove(self.log_filename)
        except:
            pass


def random_values():
    SIGMA = 2
    filter = NSigmaFilter(n=SIGMA)
    filter.configure()
    filter.valgrind_start()
    
    data = [random.gauss(5, 0.5) for _ in range(200)]
    ret = filter.send_all(data)

    filtered = json.loads(ret['body'])

    np_data = numpy.array(data)
    mean = numpy.mean(np_data)
    dev = numpy.std(np_data)

    thresh = mean + SIGMA*dev
    filt = []
    for a in data:
        if a < thresh:
            filt.append(a)
    print(len(filt), len(filtered))
    #Rounding errors may happen
    if len(filt) != len(filtered):
        logging.error("Length difference between calculated filtered data : {} != {}. Rounding imprecisions can happen".format(len(filt), len(filtered)))
        return False

    if not numpy.allclose(filt, filtered):
        logging.error("Arrays are different")
        return False
    return True

def wrong_data_empty():
    filter = NSigmaFilter(n=2)
    filter.configure()
    filter.valgrind_start()

    data = []

    ret = filter.send_all(data)

    if len(ret['body']) != 0:
        logging.error("Body Length is not 0 but should be empty")
        return False

    log_file = open(filter.log_filename, 'r')
    if log_file is None:
        logging.error("Can't open alerts log file")
        return False
    alerts = log_file.readlines()
    if len(alerts) == 0:
        logging.error("No alerts found")
        return False
    if not 'No data' in alerts[-1] or not 'nsigma' in alerts[-1]:
        logging.error('the expected alert was not on the alert log file')
        return False
    
    return True

def wrong_data_string():
    filter = NSigmaFilter(n=2)
    filter.configure()
    filter.valgrind_start()

    data = [345, '124', '2343']

    ret = filter.send_all(data)

    # data is processed without the the strings, we expect *some* results
    if len(ret['body']) == 0:
        logging.error("Body Length is 0 but shouldn't be, we expect some results")
        return False

    log_file = open(filter.log_filename, 'r')
    if log_file is None:
        logging.error("Can't open alerts log file")
        return False

    alerts = log_file.readlines()
    if len(alerts) == 0:
        logging.error("No alerts found")
        return False
    if not 'Parsing error, expecting array of numbers' in alerts[-1] or not 'nsigma' in alerts[-1]:
        logging.error('the expected alert was not on the alert log file')
        return False
    
    return True
