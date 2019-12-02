__author__ = "Hugo SOSZYNSKI"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'The heartbeat functions and main class'

from os import kill, access, F_OK


class HeartBeat:
    """
    This class is in charge of the health check for the filters.
    """

    @staticmethod
    def check_socket(file):
        try:
            if access(file, F_OK):
                return True
            return False
        except Exception:
            return False

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
