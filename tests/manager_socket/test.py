import pprint
import manager_socket.monitor_test as monitor_test
import manager_socket.update_test as update_test
import manager_socket.reporting_test as reporting_test
import manager_socket.multi_filters_test as multi_filters_test
import manager_socket.logging_test as logging_test
from tools.output import print_results

def run():
    print('Management Socket Results:')

    monitor_test.run()
    update_test.run()
    reporting_test.run()
    multi_filters_test.run()
    logging_test.run()

    print()
    print()