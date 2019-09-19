"""This file is part of Vulture 3.

Vulture 3 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Vulture 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Vulture 3.  If not, see http://www.gnu.org/licenses/.
"""
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
from signal import SIGTERM, SIGUSR1
from pprint import pprint
from JsonSocket import JsonSocket
from HeartBeat import HeartBeat
from time import sleep
import psutil

logger = logging.getLogger()


class Services:
    """
    Manage services execution, monitoring and configuration.
    """

    def __init__(self, config_file):
        """
        Constructor, load configuration pointed by config_file.

        :param config_file: Path to the filters configuration file.
        """
        self._config_file = config_file
        self._filters = {}
        self._lock = Lock()
        self.load_conf()

    def start_all(self):
        """
        Start all the filters.
        """
        with self._lock:
            for f, _ in self._filters.items():
                self.start_one(f, True)

    def rotate_logs_all(self):
        """
        Reset the logs of all the filters
        """
        with self._lock:
            for f, _ in self._filters.items():
                try:
                    self.rotate_logs_one(f, True)
                except Exception:
                    pass

    def stop_all(self):
        """
        Stop all the filters
        """
        with self._lock:
            for f, _ in self._filters.items():
                try:
                    self.stop_one(f, True)
                except Exception:
                    pass

    def restart_all(self):
        """
        Restart all the filters
        """
        self.stop_all()
        self.start_all()

    @staticmethod
    def _build_cmd(filt, name):
        """
        Build the cmd to execute a filter with Popen.

        :param filt: The filter object.
        :param name: The name of the filter.
        :return: The formatted command.
        """

        cmd = [
            filt['exec_path'],
            name,
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

        try:
            log_level = filt['log_level'].lower()

            if log_level == "debug":
                cmd.append('-d')
            elif log_level == "info":
                cmd.append('-i')
            elif log_level == "warning":
                cmd.append('-w')
            elif log_level == "error":
                cmd.append('-e')
            elif log_level == "critical":
                cmd.append('-c')
            elif log_level == "developer":
                cmd.append('-z')
            else:
                logger.warning(
                    'Invalid log level argument provided: "{log_level}". Ignoring'.format(log_level)
                )
        except KeyError:
            pass

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

    def start_one(self, name, no_lock=False):
        """
        Start the filter named name.

        :param name: The name of the filter to start.
        :param no_lock: If set to True, will not lock mutex
        """

        if not no_lock:
            self._lock.acquire()

        cmd = self._build_cmd(self._filters[name], name)
        call(['ln', '-s', self._filters[name]['socket'],
              self._filters[name]['socket_link']])

        if not no_lock:
            self._lock.release()

        # start process
        logger.debug("Starting {}".format(" ".join(cmd)))
        self._filters[name]['status'] = psutil.STATUS_WAKING
        p = Popen(cmd)
        try:
            p.wait(timeout=1)
        except TimeoutExpired:
            if self._filters[name]['log_level'].lower() == "debug":
                logger.debug("Debug mode enabled. Ignoring timeout at process startup.")
            else:
                logger.error("Error starting filter. Did not daemonize before timeout. Killing it.")
                p.kill()
                p.wait()
                self._filters[name]['status'] = psutil.STATUS_DEAD
                return

        self._filters[name]['status'] = psutil.STATUS_RUNNING

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

    def rotate_logs_one(self, name, no_lock=False):
        """
        Rotate the logs of the filter named name.

        :param name: The name of the filter to rotate_logs.
        :param no_lock: If set to True, will not lock mutex
        """
        if not no_lock:
            self._lock.acquire()

        self.rotate_logs(name, self._filters[name]['pid_file'])

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
        try:
            Services._kill_with_pid_file(pid_file)
        except FileNotFoundError as e:
            logger.warning("No PID found for {}. Did the filter start/crash?".format(name))
            for proc in psutil.process_iter(attrs=["cmdline", "name", "pid"]):
                if name in proc.info["cmdline"] and "darwin_" in proc.info["name"]:
                    logger.warning("Filter {} found in running processes (pid {}). Killing.".format(name, proc.info['pid']))
                    kill(proc.info['pid'], SIGTERM)
                    sleep(2)

        except Exception as e:
            logger.error("Cannot stop filter {}: {}".format(name, e))

        if not socket_link:
            return
        try:
            remove(socket_link)
        except FileNotFoundError as e:
            logger.warning("Filter {} not running (no symlink found). Did the filter start/crash?".format(name))
        except Exception as e:
            logger.error("Cannot remove filter symlink {}: {}".format(name, e))

    def stop_one(self, name, no_lock=False):
        """
        Stop the filter named name.

        :param name: The name of the filter to stop.
        :param no_lock: If set to True, will not lock mutex
        """
        if not no_lock:
            self._lock.acquire()

        self.stop(name, self._filters[name]['pid_file'],
                  self._filters[name]['socket_link'])

        self._filters[name]['status'] = psutil.STATUS_STOPPED

        if not no_lock:
            self._lock.release()

    def restart_one(self, name, no_lock=False):
        """
        Restart the filter named name.

        :param name: The name of the filter to restart.
        :param no_lock: If set to True, will not lock mutex
        """
        try:
            self.stop_one(name, no_lock=no_lock)
        except Exception as e:
            logger.error("Cannot stop filter {}: {}".format(name, e))

        sleep(1)
        try:
            self.clean_one(name, no_lock=no_lock)
        except Exception as e:
            logger.error("Cannot clean {}: {}".format(name, e))

        try:
            self.start_one(name, no_lock=no_lock)
        except Exception as e:
            logger.error("Cannot start filter {}: {}".format(name, e))

        ret = Services._update_check_process(self._filters[name])
        if ret:
            logger.error("Error when starting filter {}: {}".format(name, ret))
            self.clean_one(name, no_lock=no_lock)
        else:
            self._filters['status'] = psutil.STATUS_RUNNING
            self._filters['failures'] = 0

    def update(self, names):
        """
        Update the filters which name are contained in names
        configuration and process.

        :param names: A list containing the names of the filter to update.
        :return A empty list on success. A list containing error messages on failure.
        """
        logger.debug("Update: Trying to open config file")
        try:
            with open(self._config_file, 'r') as f:
                logger.debug("Update: Loading config")
                filters = json.load(f)
        except Exception as e:
            logger.error("Unable to load conf on update: {0}".format(e))
            return [{"filter": "all", "error": "Unable to load conf on update: {0}".format(e)}]

        logger.info("Update: Configuration loaded")

        with self._lock:
            errors = []
            new = {}
            for n in names:
                try:
                    new[n] = filters[n]
                except KeyError:
                    try:
                        self.stop_one(n, no_lock=True)
                        self.clean_one(n, no_lock=True)
                    except KeyError:
                        errors.append({"filter": n,
                                       "error": 'Filter not existing'})
                    self._filters.pop(n, None)
                    continue
                try:
                    new[n]['extension'] = '.1' if self._filters[n]['extension'] == '.2' else '.2'
                except KeyError:
                    new[n]['extension'] = '.1'

                new[n]['pid_file'] = '/var/run/darwin/{name}{extension}.pid'.format(
                    name=n, extension=new[n]['extension']
                )

                new[n]['output'] = new[n]['output']

                if not new[n]['next_filter']:
                    new[n]['next_filter_unix_socket'] = 'no'
                else:
                    new[n]['next_filter_unix_socket'] = '/var/sockets/darwin/{next_filter}.sock'.format(
                        next_filter=new[n]['next_filter']
                    )

                new[n]['socket'] = '/var/sockets/darwin/{name}{extension}.sock'.format(
                    name=n, extension=new[n]['extension']
                )

                new[n]['socket_link'] = '/var/sockets/darwin/{name}.sock'.format(name=n)

                new[n]['monitoring'] = '/var/sockets/darwin/{name}_mon{extension}.sock'.format(
                    name=n, extension=new[n]['extension']
                )

                if "config_file" not in new[n]:
                    logger.warning("Field 'config_file' not found for {}: setting default.".format(n))
                    new[n]['config_file'] = '/home/darwin/conf/{name}.conf'.format(name=n)

                if 'cache_size' not in new[n]:
                    new[n]['cache_size'] = 0

                    logger.info('No cache size provided. Setting it to {cache_size}'.format(
                        cache_size=new[n]['cache_size']
                    ))

                if 'output' not in new[n]:
                    new[n]['output'] = 'NONE'
                    logger.info('No output type provided. Setting it to {output}'.format(output=new[n]['output']))

                if 'nb_thread' not in new[n]:
                    new[n]['nb_thread'] = 5

                    logger.info('No number of threads provided. Setting it to {nb_thread}'.format(
                        nb_thread=new[n]['nb_thread']
                    ))

                if 'threshold' not in new[n]:
                    new[n]['threshold'] = 101

                    logger.info('No threshold provided. Setting it to the filter\'s default threshold')

            for n, c in new.items():
                c['status'] = psutil.STATUS_WAKING
                c['failures'] = 0
                cmd = self._build_cmd(c, n)
                logger.info("Starting updated filter")
                p = Popen(cmd)
                try:
                    p.wait(timeout=1)
                except TimeoutExpired:
                    if self._filters[n]['log_level'].lower() == "debug":
                        logger.debug("Debug mode enabled. Ignoring timeout at process startup.")
                    else:
                        logger.error("Error starting filter. Did not daemonize before timeout. Killing it.")
                        p.kill()
                        p.wait()
                ret = Services._update_check_process(c)
                if ret:
                    logger.error("Unable to update filter {}: {}".format(n, ret))
                    # Then there is an error
                    errors.append({"filter": n, "error": ret})
                    try:
                        self.stop(n, c['pid_file'])
                        self.clean_one(n, no_lock=True)
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
                        self.clean_one(n, no_lock=True)
                    except Exception:
                        pass
                    continue

                try:
                    if self._filters[n]['status'] != psutil.STATUS_STOPPED:
                        logger.info("Killing older filter...")
                        Services.stop(n, self._filters[n]['pid_file'])
                        logger.debug("{} with PID {} killed.".format(n, self._filters[n]['pid_file']))
                        self.clean_one(n, no_lock=True)
                        logger.debug("Cleaned filter")
                except KeyError:
                    logger.debug("Key error when trying to kill with PID")
                    pass
                self._filters[n] = deepcopy(c)
                logger.info("successfully updated {}".format(n))

        return errors

    def load_conf(self):
        """
        Load the configuration.
        """

        with self._lock:
            logger.debug("Trying to open config file")
            try:
                with open(self._config_file, 'r') as f:
                    logger.debug("Loading config")
                    self._filters = json.load(f)
            except Exception as e:
                logger.critical("Unable to load initial conf: {0}".format(e))
                raise e

            logger.info("Configuration loaded")

            for f, c in self._filters.items():
                if 'next_filter' not in c:
                    c['next_filter'] = ''

                    logger.debug('No next filter provided')

                try:
                    c['failures'] = 0
                    c['status'] = psutil.STATUS_WAKING
                    c['extension'] = '.1'
                    c['pid_file'] = '/var/run/darwin/{filter}{extension}.pid'.format(filter=f, extension=c['extension'])

                    if not c['next_filter']:
                        c['next_filter_unix_socket'] = 'no'
                    else:
                        c['next_filter_unix_socket'] = '/var/sockets/darwin/{next_filter}.sock'.format(
                            next_filter=c['next_filter']
                        )

                    c['socket'] = '/var/sockets/darwin/{filter}{extension}.sock'.format(filter=f, extension=c['extension'])
                    c['socket_link'] = '/var/sockets/darwin/{filter}.sock'.format(filter=f)

                    c['monitoring'] = '/var/sockets/darwin/{filter}_mon{extension}.sock'.format(
                        filter=f, extension=c['extension']
                    )

                    if 'config_file' not in c:
                        logger.warning('Field "config_file" not found for {filter}: setting default'.format(filter=f))
                        c['config_file'] = '/home/vlt-sys/darwin/conf/{filter}.conf'.format(filter=f)

                    if 'cache_size' not in c:
                        c['cache_size'] = 0

                        logger.info('No cache size provided. Setting it to {cache_size}'.format(
                            cache_size=c['cache_size']
                        ))

                    if 'output' not in c:
                        c['output'] = 'NONE'
                        logger.info('No output type provided. Setting it to {output}'.format(output=c['output']))

                    if 'nb_thread' not in c:
                        c['nb_thread'] = 5

                        logger.info('No number of threads provided. Setting it to {nb_thread}'.format(
                            nb_thread=c['nb_thread']
                        ))
                except KeyError as e:
                    logger.critical("Missing parameter: {}".format(e))
                    raise e

                if 'threshold' not in c:
                    c['threshold'] = 101

                    logger.info('No threshold provided. Setting it to the filter\'s default threshold')

    def print_conf(self):
        """
        Pretty Print the configuration.
        Mostly used for debug purpose.
        """
        pprint(self._filters)

    @staticmethod
    def monitor_one(file):
        """
        Get the monitoring info from the filter named 'name'.

        :param name: The name of the filter to connect to.
        :return: Acquired monitoring data on success, None otherwise.
        """
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            logger.debug("connecting to monitoring socket...")
            sock.connect(file)
        except Exception as e:
            logger.error("Error, cannot connect to monitoring socket {} : {}".format(file, e))
            return

        try:
            #TODO implement select() method to get timeout errors and not hang
            data = JsonSocket(sock).recv()
            logger.debug("received data: '{}'".format(data))
        except Exception as e:
            logger.error("Error, filter monitoring data not received: {}".format(e))
            return
        return data

    def monitor_all(self):
        """
        Get monitoring data from all the filters.

        :return: A dict containing the monitoring data.
        """
        monitor_data = {}
        with self._lock:
            for n, c in self._filters.items():
                monitor_data[n] = Services.monitor_one(c['monitoring'])
        return monitor_data

    @staticmethod
    def _update_check_process(content):
        """
        Check that the passed process is up.

        :param content: Dict containing the filter configuration.
        :return: None on success, a str containing the error message on error.
        """
        logger.debug("entered _update_check_process")
        retries = 1
        status = "Not None"
        while status and retries <= 3:
            sleep(0.2)
            retries += 1
            status = None
            pid = HeartBeat.check_pid_file(content['pid_file'])
            if not pid:
                status = "PID file not accessible"
                continue

            if not HeartBeat.check_process(pid):
                status = "Process not running"
                continue

            if not HeartBeat.check_socket(content['socket']):
                status = "Main socket not created"
                continue

            if not HeartBeat.check_socket(content['monitoring']):
                status = "Monitoring socket not created"
                continue

            if Services.monitor_one(content['monitoring']) is None:
                status = "Monitoring socket not ready"
                continue

        return status

    def hb_one(self, name, content):
        """
        Perform HB to the passed and restart it if needed.

        :param name: The name of the filter.
        :param content: The configuration of the filter.
        """

        try:
            pid = HeartBeat.check_pid_file(content['pid_file'])
            if not pid:
                raise Exception('PID not accessible')

            if not HeartBeat.check_process(pid):
                raise Exception('Process not running')

            if not HeartBeat.check_socket(content['socket']):
                raise Exception('Socket not accessible')

        except Exception as e:
            logger.warning("HeartBeat failed on {} ({}). Restarting the filter.".format(name, e))
            self._filters[name]['failures']  += 1
            if self._filters[name]['failures'] > 3:
                logger.error('Too many failures when starting {}. Filter stopped.'.format(name))
                self.stop_one(name, no_lock=True)
                sleep(1)
                self.clean_one(name, no_lock=True)
                return
            self._filters[name]['status'] = psutil.STATUS_DEAD
            self.restart_one(name, no_lock=True)

    def hb_all(self):
        """
        Loop over all the filters to check heartbeat.
        Restart the filter in case of error.
        """

        with self._lock:
            for n, c in self._filters.items():
                if c['status'] != psutil.STATUS_STOPPED:
                    self.hb_one(n, c)

    def clean_one(self, name, no_lock=False):
        if not no_lock:
            self._lock.acquire()
        try:
            if access(self._filters[name]['pid_file'], F_OK):
                remove(self._filters[name]['pid_file'])
        except Exception as e:
            logger.error("Cannot delete leftover pidfile: {}".format(e))

        try:
            if access(self._filters[name]['socket'], F_OK):
                remove(self._filters[name]['socket'])
        except Exception as e:
            logger.error("Cannot delete leftover socket: {}".format(e))

        try:
            if access(self._filters[name]['monitoring'], F_OK):
                remove(self._filters[name]['monitoring'])
        except Exception as e:
            logger.error("Cannot delete leftover monitor socket: {}".format(e))

        if not no_lock:
            self._lock.release()

    def clean_all(self):
        with self._lock:
            for n, _ in self._filters.items():
                self.clean_one(n, no_lock=True)
