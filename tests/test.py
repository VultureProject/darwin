import logging
import argparse
from sys import stderr
import manager_socket.test as manager_socket
import core.test as core
import filters.test as filters

# `conf` is imported whole because variables inside 
# may be modified by the command line
import conf

parser = argparse.ArgumentParser()
socket_arg_group = parser.add_mutually_exclusive_group(required=False)
socket_arg_group.add_argument("--tcp", action="store_true")
socket_arg_group.add_argument("--udp", action="store_true")
socket_arg_group.add_argument("--unix", action="store_true")
parser.add_argument("ip_address", nargs='?', 
    help="(For tcp and udp only) The IP address formatted like this: ipv4: 'x.x.x.x:port', ipv6: '[x:x:x:x:x:x:x]:port'")
parser.add_argument("--valgrind", action='store_true', help="Run valgrind on the filters")
args = parser.parse_args()

if args.tcp:
    conf.DEFAULT_PROTOCOL = 'tcp'
elif args.udp:
    conf.DEFAULT_PROTOCOL = 'udp'
elif args.unix:
    conf.DEFAULT_PROTOCOL = 'unix'

if args.ip_address != None:
    conf.DEFAULT_ADDRESS = args.ip_address

if args.valgrind:
    conf.VALGRIND_MEMCHECK = True

if __name__ == "__main__":
    logging.basicConfig(filename="test_error.log", filemode='w', level=logging.ERROR)

    core.run()
    filters.run()
    manager_socket.run()

    print("Note: you can read test_error.log for more details", file=stderr)
