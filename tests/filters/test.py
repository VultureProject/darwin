import threading

import filters.flogs as flogs
import filters.fconnection as fconnection
import filters.fhostlookup as fhostlookup
import filters.fdga as fdga
from tools.output import print_results

def run():
    print('Filter Results:')

    flist = [
        #fdga,
        fconnection,
        flogs,
        fhostlookup,
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
