import subprocess
import os
import logging
from time import sleep
from conf import DEFAULT_FILTER_PATH


class Filter:

    def __init__(self, path=None, config_file=None, filter_name="filter", socket_path=None, monitoring_socket_path=None, pid_file=None, output="NONE", next_filter_socket_path="no", nb_thread=1, cache_size=0, thresold=101, log_level="-z"):
        self.filter_name = filter_name
        self.socket = socket_path if socket_path else "/tmp/{}.sock".format(filter_name)
        self.config = config_file if config_file else "/tmp/{}.conf".format(filter_name)
        self.path = path if path else "{}darwin_{}".format(DEFAULT_FILTER_PATH, filter_name)
        self.monitor = monitoring_socket_path if monitoring_socket_path else "/tmp/{}_mon.sock".format(filter_name)
        self.pid = pid_file if pid_file else "/tmp/{}.pid".format(filter_name)
        self.cmd = [self.path, self.filter_name, self.socket, self.config, self.monitor, self.pid, output, next_filter_socket_path, str(nb_thread), str(cache_size), str(thresold), log_level]
        self.process = None

    def __del__(self):
        if self.process and self.process.poll() is None:
            self.stop()
        self.clean_files()

    def start(self):
        self.process = subprocess.Popen(self.cmd)
        sleep(1)

    def stop(self):
        ret = True
        self.process.terminate()
        try:
            self.process.wait(3)
        except subprocess.TimeoutExpired:
            logging.error("Unable to stop filter properly... Killing it.")
            self.process.kill()
            self.process.wait()
            return False
        return True

    def clean_files(self):

        try:
            os.remove(self.config)
        except:
            pass

        try:
            os.remove(self.socket)
        except:
            pass

        try:
            os.remove(self.monitor)
        except:
            pass

        try:
            os.remove(self.pid)
        except:
            pass

        """
        try:
            os.remove(self.path)
        except:
            pass
        """


    def configure(self, config):
        with open(self.config, 'w') as f:
            f.write(config)
