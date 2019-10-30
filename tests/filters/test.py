import filters.flogs as flogs
import filters.fconnection as fconnection
import filters.fhostlookup as fhostlookup
import filters.fdga as fdga
from tools.output import print_results


def run():
    print('Filter Results:')

    fdga.run()
    fconnection.run()
    flogs.run()
    fhostlookup.run()

    print()
    print()
