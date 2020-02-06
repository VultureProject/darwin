import threading

import filters.fdga as fdga
import filters.flogs as flogs
import filters.fsofa as fsofa
import filters.fanomaly as fanomaly
import filters.ftanomaly as ftanomaly 
import filters.fconnection as fconnection
import filters.fhostlookup as fhostlookup
from tools.output import print_results

def run():
    print('Filter Results:')

    flist = [
        fdga,
        fconnection,
        flogs,
        fhostlookup,
        ftanomaly,
        fsofa,
        fanomaly
    ]

    threads = list()
    for f in flist:
        t = threading.Thread(target=f.run)
        threads.append(t)
        t.start()

    for thread in threads:
        thread.join()

    print()
    print()
