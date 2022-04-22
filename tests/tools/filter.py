import json
import logging
import os
import os.path
import subprocess

# `conf` is imported whole because variables inside 
# may be modified by the command line
import conf
from darwin import DarwinApi
from time import sleep
from tools.redis_utils import RedisServer

DEFAULT_LOG_FILE = "/var/log/darwin/darwin.log"
DEFAULT_ALERTS_FILE = "/var/log/darwin/alerts.log"
REDIS_SOCKET = "/tmp/redis.sock"
REDIS_LIST_NAME = "darwin_tests"
REDIS_CHANNEL_NAME = "darwin.tests"

class Filter():

    def __init__(self, path=None, config_file=None, filter_name="filter", socket_path=None, monitoring_socket_path=None, pid_file=None, output="NONE", next_filter_socket_path="no", nb_threads=1, cache_size=0, threshold=101, log_level="DEVELOPER", socket_type=None):
        self.filter_name = filter_name
        self.process = None
        self.set_socket_info(socket_type, socket_path)
        self.config = config_file if config_file else "{}/{}.conf".format(conf.TEST_FILES_DIR, filter_name)
        self.path = path if path else "{}darwin_{}".format(conf.DEFAULT_FILTER_PATH, filter_name)
        self.monitor = monitoring_socket_path if monitoring_socket_path else "{}/{}_mon.sock".format(conf.TEST_FILES_DIR, filter_name)
        self.pid = pid_file if pid_file else "{}/{}.pid".format(conf.TEST_FILES_DIR, filter_name)
        self.cmd = [self.path, "-l", log_level, self.filter_name, self.socket, self.config, self.monitor, self.pid, output, next_filter_socket_path, str(nb_threads), str(cache_size), str(threshold)]
        if self.socket_type.startswith('udp'):
            # Flags MUST be before positional arguments as the parsing on HardenedBSD is not done on all the arguments
            # On BSD getopt stops at the first argument which is not in the specified flags
            self.cmd.insert(1, '-u')
        self.error_code = 99 # For valgrind testing
        self.pubsub = None
        self.prepare_log_file()

    def set_socket_info(self, socket_type, socket_path_address):
        self.socket_type = socket_type if socket_type else conf.DEFAULT_PROTOCOL
        if self.socket_type == 'unix':
            self.socket = socket_path_address if socket_path_address else "{}/{}.sock".format(conf.TEST_FILES_DIR, self.filter_name)
            self.host = None
            self.port = -1
        elif self.socket_type == 'tcp' or self.socket_type == 'udp':
            self.socket = socket_path_address if socket_path_address else conf.DEFAULT_ADDRESS
            splitted_address = self.socket.rsplit(':', 1)
            if '[' in splitted_address[0]: # ipv6 address
                self.socket_type += '6'
                self.host = splitted_address[0][1:-1]
            else:
                self.host = splitted_address[0]
            self.port = int(splitted_address[1])
        else:
            raise NotImplementedError("Unrecognized protocol : '{}'".format(self.socket_type))

    def get_darwin_api(self, verbose=False, timeout=None):
        return DarwinApi(socket_type=self.socket_type, socket_path=self.socket, socket_host=self.host, socket_port=self.port, verbose=verbose, timeout=timeout)

    def prepare_log_file(self):
        # TODO variabilize once path can be changed
        self.log_file = open("/var/log/darwin/darwin.log", 'a+')
        if self.log_file is None:
            logging.error("Could not open darwin.log")
            return False

    def __del__(self):
        if self.process and self.process.poll() is None:
            if conf.VALGRIND_MEMCHECK is False:
                self.stop()
            else:
                self.valgrind_stop()
        self.clean_files()
        try:
            if self.log_file is not None and not self.log_file.closed:
                self.log_file.close()
        except AttributeError:
            pass

    def check_start(self):
        if not os.path.exists(self.pid):
            logging.error("No PID file at start, maybe filter crashed")
            return False
        p = self.process.poll()
        if p is not None:
            logging.error("Process does not respond, must be down")
            return False
        return True

    def check_stop(self):
        if os.path.exists(self.pid):
            logging.error("PID file not removed when stopping, maybe filter crashed")
            return False
        return True

    def start(self):
        self.process = subprocess.Popen(self.cmd)
        sleep(2)
        return self.check_start()

    def check_run(self):
        if self.process and self.process.poll() is None:
            return True

        return False

    def stop(self):
        self.process.terminate()
        try:
            self.process.wait(3)
        except subprocess.TimeoutExpired:
            logging.error("Unable to stop filter properly... Killing it.")
            self.process.kill()
            self.process.wait()
            return False
        return self.check_stop()

    def valgrind_start(self):
        if conf.VALGRIND_MEMCHECK is False:
            return self.start()
        command = ['valgrind',
                   '--tool=memcheck',
                   '--leak-check=full',
                   '--errors-for-leak-kinds=definite',
                   '-q', # Quiet mode so it doesn't pollute the output
                   '--error-exitcode={}'.format(self.error_code), #If valgrind reports error(s) during the run, return this exit code.
                   ] + self.cmd

        self.process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        sleep(3)
        return self.check_start()

    def valgrind_stop(self):
        if conf.VALGRIND_MEMCHECK is False:
            return self.stop()
        self.process.terminate()
        try:
            self.process.wait(15) # Valgrind can take some time, so it needs a generous timeout
        except subprocess.TimeoutExpired:
            logging.error("Unable to stop filter properly... Killing it.")
            self.process.kill()
            self.process.wait()
            return False

        # If valgrind return the famous error code
        out, err = self.process.communicate()
        if self.process.returncode == self.error_code :
            print("\033[91m[Valgrind error(s)]\33[0m", end='', flush=True)
            logging.error("Valgrind returned error code: {}".format(self.process.returncode))
            logging.error("Valgrind error: {}".format(err.decode()))
            return False
        elif err:
            print("\33[33m[Valgrind warning(s)]\33[0m", end='', flush=True)
            logging.error("Valgrind warning: {}".format(err.decode()))

        return self.check_stop()

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


    def configure(self, config):
        with open(self.config, 'w') as f:
            f.write(config)


    def send_single(self, line):
        """
        Send a single line.
        """
        api = self.get_darwin_api()
        ret = api.call(line, response_type="back")

        api.close()
        return ret


    def validate_alert_line(self, alert_line, details_checks=[]):
        try:
            # Check if the alert is a valid json
            json_line = json.loads(alert_line)
        except Exception as e:
            logging.error("filter: alert_line is not a valid json: {}".format(e))
            return False

        for key, key_type in [
            ("alert_type", "str"),
            ("alert_subtype", "str"),
            ("alert_time", "str"),
            ("level", "str"),
            ("rule_name", "str"),
            ("tags", "list"),
            ("entry", "str"),
            ("score", "int"),
            ("evt_id", "str"),
            ("details", "dict"),
            ]:
            # Check if key is present in alert
            if key not in json_line:
                logging.error("filter: key '{}' not in alert line".format(key))
                logging.error("filter: alert line is {}".format(alert_line))
                return False

            # Check if the key is the correct type
            real_key_type = type(json_line[key]).__name__
            if real_key_type != key_type:
                logging.error("filter: alert key '{}' is not the expected type, got '{}' but expected '{}'".format(key, real_key_type, key_type))
                logging.error("filter: alert line is\n{}".format(alert_line))
                return False

            if key == "details" and details_checks:
                for subkey, subkey_type in details_checks:
                    # Check if key is present in alert
                    if subkey not in json_line[key]:
                        logging.error("filter: key '{}' not in alert details line".format(subkey))
                        logging.error("filter: alert line is\n{}".format(alert_line))
                        return False

                    # Check if the subkey is the correct type
                    real_subkey_type = type(json_line[key][subkey]).__name__
                    if real_subkey_type != subkey_type:
                        logging.error("filter: alert details key '{}' is not the expected type, got '{}' but expected '{}'".format(subkey, real_subkey_type, subkey_type))
                        logging.error("filter: alert line is\n{}".format(alert_line))
                        return False


        return True
    def check_line_in_filter_log(self, line, keep_init_pos=True):
        found = False
        if self.log_file is None or self.log_file.closed:
            logging.error("check_line_in_filter_log: cannot use log file to check line {}".format(line))
            return False

        init_pos = self.log_file.tell()
        file_line = self.log_file.readline()
        while file_line:
            if line in file_line:
                found = True
                break
            file_line = self.log_file.readline()

        # Go back to initial position in file
        if keep_init_pos:
            self.log_file.seek(init_pos, 0)

        return found
