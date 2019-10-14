import filters.fconnection as fconnection
import filters.fyarascan as fyarascan
from tools.output import print_results


def run():
    print('Filter Results:')

    fconnection.run()
    fyarascan.run()

    print()
    print()
