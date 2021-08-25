import json
import logging
import os
import numpy
from time import sleep

from numpy.testing._private.utils import assert_array_equal

from tools.filter import Filter
from tools.output import print_result

def run():
    tests = [
        one_day_data
    ]

    for i in tests:
        print_result("pcr: " + i.__name__, i)

def one_day_data():
    filter = Filter(filter_name="pcr")

    filter.configure('{ }')

    filter.valgrind_start()
    
    f = open('filters/data/PCR_1day_data_formated.json', 'r')
    data = json.loads(''.join(f.readlines()))
    f.close()

    pcrs = json.loads(filter.send_all(data)['body'])

    comp = [(float(val[1]) - float(val[0])) /(float(val[0]) + float(val[1])) for val in data]

    return len(comp) == len(pcrs) and numpy.allclose(pcrs, comp,rtol=0.01)

