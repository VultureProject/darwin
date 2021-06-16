__author__ = "Hugo SOSZYNSKI"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'Services / filters management'

import logging
import json
import socket
from threading import Lock
from copy import deepcopy
from subprocess import Popen, call, TimeoutExpired
from os import kill, remove, access, F_OK
from signal import SIGTERM, SIGUSR1, SIGKILL
from pprint import pprint
from JsonSocket import JsonSocket
from HeartBeat import HeartBeat
from time import sleep
import psutil
from config import load_conf, ConfParseError

logger = logging.getLogger()


class Services:
    """
    Manage services execution, monitoring and configuration.
    """

    def __init__(self, filters):
        """
        Constructor, load configuration pointed by config_file.

        :param config_file: Path to the filters configuration file.
        """
        self._filters = deepcopy(filters)
        self._lock = Lock()

    def start_all(self):
        """
        Start all the filters.
        """
        for _, filter in self._filters.items():
            if self.start_one(filter, True):
                ret = Services._wait_process_ready(filter)
                if ret:
                    logger.error("Error when starting filter {}: {}".format(filter['name'], ret))
                    self.stop_one(filter, no_lock=True)
                    self.clean_one(filter, no_lock=True)
                else:
                    logger.debug("Linking UNIX sockets...")
                    filter['status'] = psutil.STATUS_RUNNING
                    call(['ln', '-s', filter['socket'], filter['socket_link']])

    def rotate_logs_all(self):
        """
        Reset the logs of all the filters
        """
        with self._lock:
            for _, filter in self._filters.items():
                try:
                    self.rotate_logs_one(filter, True)
                except Exception:
                    pass

    def stop_all(self):
        """
        Stop all the filters
        """
        for _, filter in self._filters.items():
            try:
                self.stop_one(filter, True)
                logger.debug("stop_all: after stop_one")
                self.clean_one(filter, True)
                logger.debug("stop_all: after clean_one")
            except Exception:
                pass

    @staticmethod
    def _build_cmd(filt):
        """
        Build the cmd to execute a filter with Popen.

        :param filt: The filter object.
        :return: The formatted command.
        """

        cmd = [filt['exec_path']]

        try:
            if filt['log_level'] not in ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL", "DEVELOPER"]:
                logger.warning(
                    'Invalid log level argument provided: "{log_level}". Ignoring'.format(filt['log_level'])
                )
            else:
                cmd.append('-l')
                cmd.append(filt['log_level'])
        except KeyError:
            pass

        cmd += [
            filt['name'],
            filt['socket'],
            filt['config_file'],
            filt['monitoring'],
            filt['pid_file'],
            filt['output'],
            filt['next_filter_unix_socket'],
            str(filt['nb_thread']),
            str(filt['cache_size']),
            str(filt['threshold']),
        ]

        return cmd

    @staticmethod
    def _rotate_logs_with_pid_file(file):
        """
        Send SIGUSR1 signal to a program having his PID in file.

        :param file: The file containing the PID a a program.
        """
        with open(file) as f:
            pid = f.readline()
        kill(int(pid), SIGUSR1)

    @staticmethod
    def _kill_with_pid_file(file):
        """
        Send SIGTERM signal to a program having his PID in file.

        :param file: The file containing the PID a a program.
        """
        logger.debug("Entering kill_with_pid_file")
        if not access(file, F_OK):
            logger.warning("Trying to read non-existing PID file...")
            return
        with open(file, 'r') as f:
            pid = f.readline()
        logger.debug("Pid read : {}".format(pid))
        kill(int(pid), SIGTERM)
        logger.debug("SIGTERM sent")
        return int(pid)

    def start_one(self, filter, no_lock=False):
        """
        Start the filter.

        :param filter: The dict of the filter to start.
        :param no_lock: If set to True, will not lock mutex
        """

        if not no_lock:
            self._lock.acquire()

        cmd = self._build_cmd(filter)

        if not no_lock:
            self._lock.release()

        # start process
        logger.debug("Starting {}".format(" ".join(cmd)))
        filter['status'] = psutil.STATUS_WAKING

        try:
            p = Popen(cmd)
        except OSError as e:
            logger.error("cannot start filter: " + str(e))
            filter['status'] = psutil.STATUS_DEAD
            return False
        if filter['log_level'].lower() != "debug":
            try:
                p.wait(timeout=10)
            except TimeoutExpired:
                logger.error("Error starting filter. Did not daemonize before timeout. Killing it.")
                p.kill()
                p.wait()
                filter['status'] = psutil.STATUS_DEAD
                return False

        return True

    @staticmethod
    def rotate_logs(name, pid_file):
        """
        Rotate the filter logs based on his pid_file.

        :param name: The name of the filter.
        :param pid_file: The pid file of the filter.
        """
        try:
            Services._rotate_logs_with_pid_file(pid_file)
        except Exception as e:
            logger.error("Cannot rotate logs of filter {}: {}".format(name, e))

    def rotate_logs_one(self, filter, no_lock=False):
        """
        Rotate the logs of the filter.

        :param filter: The dict of the filter to rotate_logs.
        :param no_lock: If set to True, will not lock mutex
        """
        if not no_lock:
            self._lock.acquire()

        self.rotate_logs(filter['name'], filter['pid_file'])

        if not no_lock:
            self._lock.release()

    @staticmethod
    def stop(name, pid_file, socket_link=None):
        """
        Stop the filter based on his pid_file and
        remove the associated socket symlink (if provided).

        :param name: The name of the filter.
        :param pid_file: The pid file of the filter.
        :param socket_link: The symlink to the filter socket (Optional).
        """
        pid = None

        try:
            pid = Services._kill_with_pid_file(pid_file)
        except FileNotFoundError as e:
            logger.warning("No PID file found for {}. Did the filter start/crash?".format(name))
            for proc in psutil.process_iter(attrs=["cmdline", "name", "pid"]):
                if name in proc.info["cmdline"] and "darwin_" in proc.info["name"]:
                    logger.warning("Filter {} found in running processes (pid {}). Killing.".format(name, proc.info['pid']))
                    kill(proc.info['pid'], SIGTERM)
                    pid = proc.info['pid']

        except Exception as e:
            logger.error("Cannot stop filter {}: {}".format(name, e))

        if pid:
            tries = 0
            proc_running = HeartBeat.check_process(pid)
            while tries < 10 and proc_running:
                sleep(1)
                proc_running = HeartBeat.check_process(pid)
                tries += 1

            if proc_running:
                logger.warning("filter {} did not stop, forcing.".format(name))
                kill(int(pid), SIGKILL)

        if not socket_link:
            return
        try:
            remove(socket_link)
        except FileNotFoundError as e:
            logger.warning("Filter {} not running (no symlink found). Did the filter start/crash?".format(name))
        except Exception as e:
            logger.error("Cannot remove filter symlink {}: {}".format(name, e))

    def stop_one(self, filter, no_lock=False):
        """
        Stop the filter.

        :param filter: The dict of the filter to stop.
        :param no_lock: If set to True, will not lock mutex
        """
        if not no_lock:
            self._lock.acquire()

        filter['status'] = psutil.STATUS_TRACING_STOP

        self.stop(filter['name'], filter['pid_file'],
                  filter['socket_link'])

        filter['status'] = psutil.STATUS_STOPPED

        if not no_lock:
            self._lock.release()

    def restart_one(self, filter, no_lock=False):
        """
        Restart the filter.

        :param filter: The object of the filter to restart.
        :param no_lock: If set to True, will not lock mutex
        """
        try:
            self.stop_one(filter, no_lock=no_lock)
        except Exception as e:
            logger.error("Cannot stop filter {}: {}".format(filter['name'], e))

        try:
            self.clean_one(filter, no_lock=no_lock)
        except Exception as e:
            logger.error("Cannot clean {}: {}".format(filter['name'], e))

        try:
            self.start_one(filter, no_lock=no_lock)
        except Exception as e:
            logger.error("Cannot start filter {}: {}".format(filter['name'], e))

        ret = Services._wait_process_ready(filter)
        if ret:
            logger.error("Error when starting filter {}: {}".format(filter['name'], ret))
            self.stop_one(filter, no_lock=no_lock)
            self.clean_one(filter, no_lock=no_lock)
        else:
            filter['status'] = psutil.STATUS_RUNNING
            call(['ln', '-s', filter['socket'], filter['socket_link']])

    def update(self, names, prefix, suffix):
        """
        Update the filters which name are contained in names
        configuration and process.

        :param names: A list containing the names of the filter to update.
        :return A empty list on success. A list containing error messages on failure.
        """
        from config import filters as conf_filters
        logger.debug("Update: Trying to open config file")
        try:
            #Reload conf, global variable 'conf_filters' will be updated
            load_conf(prefix, suffix)
        except ConfParseError as e:
            logger.error("Update: Unable to update: {}".format(e))
            return [str(e)]

        logger.info("Update: Configuration loaded")

        if not names:
            # Do a symetric diff of 2 sets, composed of the keys from current filters' dict and the dict loaded from conf
            # and unpack values as a list
            # This yields a list of the new and deleted filters (by name only) = a diff of configured filters
            names = [*(set(self._filters.keys()) ^ set(conf_filters.keys()))]

        with self._lock:
            errors = []
            new = {}
            logger.info("updating filters {}".format(names))
            for n in names:
                try:
                    new[n] = conf_filters[n]
                except KeyError:
                    try:
                        self.stop_one(self._filters[n], no_lock=True)
                        self.clean_one(self._filters[n], no_lock=True)
                    except KeyError:
                        errors.append({"filter": n,
                                       "error": 'Filter not existing'})
                    self._filters.pop(n, None)
                    continue
                try:
                    new[n]['extension'] = '.1' if self._filters[n]['extension'] == '.2' else '.2'
                except KeyError:
                    new[n]['extension'] = '.1'

                try:
                    new[n]['failures'] = self._filters[n]['failures']
                except KeyError:
                    new[n]['failures'] = 0

                new[n]['pid_file'] = '{prefix}/run{suffix}/{name}{extension}.pid'.format(
                    prefix=prefix, suffix=suffix,
                    name=n, extension=new[n]['extension']
                )

                new[n]['socket'] = '{prefix}/sockets{suffix}/{name}{extension}.sock'.format(
                    prefix=prefix, suffix=suffix,
                    name=n, extension=new[n]['extension']
                )

                new[n]['monitoring'] = '{prefix}/sockets{suffix}/{name}_mon{extension}.sock'.format(
                    prefix=prefix, suffix=suffix,
                    name=n, extension=new[n]['extension']
                )

            for n, c in new.items():
                cmd = self._build_cmd(c)
                try:
                    p = Popen(cmd)
                    p.wait(timeout=1)
                except OSError as e:
                    logger.error("cannot start filter: " + str(e))
                    c['status'] = psutil.STATUS_DEAD
                    errors.append({"filter": n, "error": "cannot start filter: {}".format(str(e))})
                    continue
                except TimeoutExpired:
                    if c['log_level'].lower() == "developer":
                        logger.debug("Debug mode enabled. Ignoring timeout at process startup.")
                    else:
                        logger.error("Error starting filter. Did not daemonize before timeout. Killing it.")
                        p.kill()
                        p.wait()
                    errors.append({"filter": n, "error": "Filter did not daemonize before timeout."})
                    continue
                ret = Services._wait_process_ready(c)
                if ret:
                    logger.error("Unable to update filter {}: {}".format(n, ret))
                    # Then there is an error
                    errors.append({"filter": n, "error": ret})
                    try:
                        self.stop(n, c['pid_file'])
                        self.clean_one(c, no_lock=True)
                    except Exception:
                        pass
                    continue

                c['status'] = psutil.STATUS_RUNNING

                logger.info("Switching filters symlink...")
                try:
                    if call(['ln', '-sfn', c['socket'], c['socket_link']]) != 0:
                        raise Exception('Unable to update filter\'s socket symlink')
                except Exception as e:
                    logger.error("Unable to link new filter {}: {}".format(n, e))
                    errors.append({"filter": n, "error": "{0}".format(e)})
                    try:
                        self.stop(n, c['pid_file'])
                        self.clean_one(c, no_lock=True)
                    except Exception:
                        pass
                    continue

                try:
                    logger.info("Killing older filter...")
                    # Call 'stop' instead of 'stop_one' to avoid deletion of socket_link
                    self.stop(n, self._filters[n]['pid_file'])
                    self.clean_one(self._filters[n], no_lock=True)
                except KeyError:
                    logger.info("no older filter to kill, finalizing...")
                    pass
                self._filters[n] = deepcopy(c)
                logger.info("successfully updated {}".format(n))

        return errors


    def print_conf(self):
        """
        Pretty Print the configuration.
        Mostly used for debug purpose.
        """
        pprint(self._filters)

    @staticmethod
    def get_proc_info(filter, proc_stats=[]):
        ret = {}
        from config import stats_reporting
        if not proc_stats:
            logger.debug("get_proc_info(): no special stats, taking ones in configuration")
            proc_stats = stats_reporting.get('proc_stats', ['memory_percent', 'cpu_percent'])

        for proc in psutil.process_iter(attrs=["cmdline", "name"]):
            if filter['name'] in proc.info["cmdline"] and "darwin_" in proc.info["name"]:
                logger.debug("get_proc_info(): found processus {}, getting stats {}".format(filter['name'], proc_stats))
                try:
                    ret.update(proc.as_dict(proc_stats))
                except Exception as e:
                    logger.error("get_proc_info(): could not get proc info -> {}".format(e))
                    pass

        return ret


    @staticmethod
    def monitor_one(file):
        """
        Get the monitoring info from the filter's monitoring socket.

        :param file: The fullpath of the management socket to connect to.
        :return: Acquired monitoring data on success, None otherwise.
        """
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            logger.debug("Connecting to monitoring socket...")
            sock.connect(file)
        except Exception as e:
            logger.warning("Cannot connect to monitoring socket {} : {}".format(file, e))
            return

        try:
            #TODO implement select() method to get timeout errors and not hang
            data = JsonSocket(sock).recv()
            logger.debug("received data: '{}'".format(data))
        except Exception as e:
            logger.error("Filter monitoring data not received: {}".format(e))
            return
        return data

    def monitor_all(self, proc_stats=[]):
        """
        Get monitoring data from all the filters.

        :return: A dict containing the monitoring data.
        """
        monitor_data = {}
        with self._lock:
            for n, c in self._filters.items():
                monitor_data[n] = Services.monitor_one(c['monitoring'])
                if monitor_data[n]:
                    monitor_data[n]['failures'] = c['failures']
                    monitor_data[n]['proc_stats'] = Services.get_proc_info(c, proc_stats)
                else:
                    monitor_data[n] = {}
                    monitor_data[n]['status'] = 'error'
        return monitor_data

    @staticmethod
    def _wait_process_ready(content):
        """
        Check that the passed process is up and running.

        :param content: Dict containing the filter configuration.
        :return: None on success, a str containing the error message on error.
        """
        logger.debug("entered _wait_process_ready")
        retries = 0
        status = "Not None"
        while status and retries < 3:
            retries += 1
            status = None
            sleep(0.2)
            pid = HeartBeat.check_pid_file(content['pid_file'])

            if not pid:
                status = "PID file not accessible"
                continue

            if not HeartBeat.check_process(pid):
                status = "Process not running"
                continue

            if not HeartBeat.check_socket(content['monitoring']):
                status = "Monitoring socket not created"
                continue

        if status:
            return status

        status = "Not None"
        retries = 0
        while status and retries < 10:
            status = None
            retries += 1
            sleep(1)
            resp = Services._get_monitoring_info(content['monitoring'])
            if not resp:
                status = "Monitoring socket not ready"
                continue

            if "running" in resp:
                if not HeartBeat.check_socket(content['socket']):
                    status = "Main socket not created"
                    continue
            else:
                status = "Filter not ready"
                continue

        return status

    def hb_one(self, filter):
        """
        Perform HB to the passed and restart it if needed.

        :param filter: The dict object representing the filter.
        """

        try:
            pid = HeartBeat.check_pid_file(filter['pid_file'])
            if not pid:
                raise Exception('PID not accessible')

            if not HeartBeat.check_process(pid):
                raise Exception('Process not running')

            if not HeartBeat.check_socket(filter['socket']):
                raise Exception('Socket not accessible')

            if not HeartBeat.check_socket(filter['monitoring']):
                raise Exception('Monitoring socket not accessible')

        except Exception as e:
            logger.warning("HeartBeat failed on {} ({}). Restarting the filter.".format(filter['name'], e))
            filter['failures'] += 1
            filter['status'] = psutil.STATUS_DEAD
            self.restart_one(filter, no_lock=True)

    def hb_all(self):
        """
        Loop over all the filters to check heartbeat.
        Restart the filter in case of error.
        """

        with self._lock:
            for _, c in self._filters.items():
                if c['status'] != psutil.STATUS_STOPPED:
                    self.hb_one(c)

    def clean_one(self, content, no_lock=False):
        if not no_lock:
            self._lock.acquire()
        try:
            if access(content['pid_file'], F_OK):
                remove(content['pid_file'])
        except Exception as e:
            logger.error("Cannot delete leftover pidfile: {}".format(e))

        try:
            if access(content['socket'], F_OK):
                remove(content['socket'])
        except Exception as e:
            logger.error("Cannot delete leftover socket: {}".format(e))

        try:
            if access(content['monitoring'], F_OK):
                remove(content['monitoring'])
        except Exception as e:
            logger.error("Cannot delete leftover monitor socket: {}".format(e))

        if not no_lock:
            self._lock.release()

    def clean_all(self):
        with self._lock:
            for _, c in self._filters.items():
                self.clean_one(c, no_lock=True)

    @staticmethod
    def _get_monitoring_info(socket_path):
        """
        Sends a request to a socket and returns the answer.
        :param request: the binary formatted request
        :param socket_path: the fullpath to the socket file
        :return: the answer as a string or None if no answer could be received
        """

        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

        try:
            sock.connect(socket_path)
        except socket.error:
            return None
        try:
            sock.sendall(b'')
        except Exception:
            return None

        try:
            response = sock.recv(4096).decode()
        except Exception:
            return None

        return response
