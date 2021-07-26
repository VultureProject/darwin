__author__ = "Hugo SOSZYNSKI"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'The heartbeat functions and main class'

from os import kill, access, F_OK
import socket

class HeartBeat:
    """
    This class is in charge of the health check for the filters.
    """

    @staticmethod
    def check_unix_socket(file):
        try:
            if access(file, F_OK):
                return True
            return False
        except Exception:
            return False

    def check_network_socket(filter_network):
        if filter_network['socket_type'] == 'UNIX':
            return HeartBeat.check_unix_socket(filter_network['address_path'])

        elif filter_network['socket_type'] == 'TCP':
            # parse ip address field
            splitted_address = filter_network['address_path'].rsplit(':', 1)
            if '[' in splitted_address[0]: # ipv6 address
                host_address = splitted_address[0][1:-1]
            else:
                host_address = splitted_address[0]
            host_port = int(splitted_address[1])
            #test connection to socket
            if ':' in host_address: # ipv6
                s = socket.socket(socket.AF_INET6)
            else:
                s = socket.socket(socket.AF_INET)
            s.settimeout(2)
            res = s.connect_ex((host_address,host_port))
            s.close()
            return res == 0
        else: #No check for UDP
            return True

    @staticmethod
    def check_pid_file(file):
        """
        Check if the PID fil exist.

        :param file: The file to check.
        :return: The pid contained by the file on success, None otherwise.
        """
        try:
            with open(file, 'r') as f:
                return int(f.read())
        except Exception:
            return None

    @staticmethod
    def check_process(pid):
        """
        Check if the process is currently running.

        :param pid: The pid of the process.
        :return: True if the process is running, False otherwise.
        """
        try:
            kill(pid, 0)
        except OSError:
            return False
        else:
            return True
