import filters.fdga as fdga
import filters.flogs as flogs
import filters.ftanomaly as ftanomaly 
import filters.fconnection as fconnection
import filters.fhostlookup as fhostlookup
from tools.output import print_results


def run():
    print('Filter Results:')

    ftanomaly.run()
    fdga.run()
    fconnection.run()
    flogs.run()
    fhostlookup.run()

    print()
    print()
