# import filters.fdga as fdga
# import filters.flogs as flogs
# import filters.fsofa as fsofa
# import filters.fanomaly as fanomaly
# import filters.ftanomaly as ftanomaly 
# import filters.fconnection as fconnection
# import filters.fhostlookup as fhostlookup
import filters.fbuffer as fbuffer
from tools.output import print_results


def run():
    print('Filter Results:')

#    ftanomaly.run()
#    fdga.run()
#    fconnection.run()
#    flogs.run()
#    fhostlookup.run()
#    fsofa.run()
#    fanomaly.run()
    fbuffer.run()

    print()
    print()
