import filters.flogs as flogs
import filters.fconnection as fconnection
from tools.output import print_results


def run():
    print('Filter Results:')

    fconnection.run()
    flogs.run()

    print()
    print()
