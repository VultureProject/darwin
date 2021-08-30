import filters.fdga as fdga
import filters.fsofa as fsofa
import filters.fanomaly as fanomaly
import filters.ftanomaly as ftanomaly
import filters.fconnection as fconnection
import filters.fhostlookup as fhostlookup
import filters.fyara as fyara
import filters.fbuffer as fbuffer
import filters.fvast as fvast
import filters.fvaml as fvaml
import filters.fsession as fsession
import filters.fpython as fpython

from tools.output import print_results


def run():
    print('Filter Results:')

    ftanomaly.run()
    fdga.run()
    fconnection.run()
    fhostlookup.run()
    fsofa.run()
    fanomaly.run()
    fyara.run()
    fbuffer.run()
    fvast.run()
    fvaml.run()
    fsession.run()
    fpython.run()

    print()
    print()
