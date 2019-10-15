import filters.flogs as flogs
import filters.fconnection as fconnection
import filters.fhostlookup as fhostlookup
from tools.output import print_results


def run():
    print('Filter Results:')

    fconnection.run()
    flogs.run()
    fhostlookup.run()

    print()
    print()
