import pprint
import manager_socket.monitor_test as monitor_test
import manager_socket.update_test as update_test
from tools.output import print_results

def run():
    results = monitor_test.run()
    results.extend(update_test.run())

    print('Management Socket Results:')
    print_results(results)
    print()
    print()