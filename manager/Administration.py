__author__ = "Hugo SOSZYNSKI"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'The unix socket administration server class'

import socket
import logging
import redis
import os
from JsonSocket import JsonSocket
from time import sleep
import json

logger = logging.getLogger()

class Server:
    """
    Manages administration connections for filter update and monitoring.
    """

    def __init__(self, prefix, suffix):
        """
        Constructor. Create the UNIX socket to listen on.
        """
        self._continue = True
        self.running = False
        self._prefix = prefix
        self._suffix = suffix
        self._socket_path = '{}/sockets{}/darwin.sock'.format(prefix, suffix)
        self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._socket.bind(self._socket_path)
        self._socket.listen(5)
        self._socket.settimeout(1)

    def __del__(self):
        """
        Close the socket.
        """
        self._socket.close()
        os.unlink(self._socket_path)

    def process(self, services, cli, cmd):
        """
        Process the incoming instruction.

        :param services: The services manager.
        :param cli: The client JsonSocket
        :param cmd: The instruction sent by the client.
        """
        response = {}
        if cmd.get('type', None):
            if cmd['type'] == 'update_filters':
                errors = services.update(cmd.get('filters', []), self._prefix, self._suffix)
                errors += self.update_stats_conf()
                if not errors:
                    response['status'] = 'OK'
                else:
                    response['status'] = 'KO'
                    response['errors'] = errors
            elif cmd['type'] == 'monitor':
                response = services.monitor_all(proc_stats=cmd.get('proc_stats', []))

        try:
            cli.send(response)
            del cli
        except Exception as e:
            logger.error("Error sending admin a response: {0}".format(e))

    def run(self, services):
        """
        Accept connections on the administration socket,
        receive the incoming commands and process them.
        """
        self.running = True
        while self._continue:
            cli = None
            try:
                _cli, _ = self._socket.accept()
            except socket.timeout:
                continue
            except Exception as e:
                logger.error("Error accepting admin connection: {0}".format(e))
                continue

            try:
                cli = JsonSocket(_cli)
                cmd = cli.recv()
            except Exception as e:
                logger.error("Error receiving data from admin: {0}".format(e))
                continue

            self.process(services, cli, cmd)

    def stop(self):
        """
        Set the stopping condition.
        """
        self._continue = False

    def constant_heartbeat(self, services, cv):
        """
        Run constant heartbeat in a thread.
        """

        with cv:
            while self._continue:
                cv.wait(1)
                if not self._continue:
                    logger.debug("HeartBeat: stopping")
                    break
                services.hb_all()

    def try_connect_redis(self, redis_conf):
        error = ""
        if redis_conf.get('ip', None):
            self._stats_redis = redis.Redis(host=redis_conf.get('ip'), port=redis_conf.get('port', 6379), health_check_interval=10)
        elif redis_conf.get('unix_path', None):
            self._stats_redis = redis.Redis(unix_socket_path=redis_conf.get('unix_path'), health_check_interval=10)
        else:
            self._stats_redis = None

        if self._stats_redis:
            try:
                self._stats_redis.ping()
            except redis.exceptions.ConnectionError as e:
                error = "Could not connect to Redis server: {}".format(e)
                logger.error(error)
                self._stats_redis = None

        return error

    def try_open_file(self, file_conf):
        error = ""
        filepath = file_conf.get('filepath', None)
        permissions = str(file_conf.get('permissions', 640))
        if filepath:
            try:
                with open(os.open(filepath, os.O_WRONLY | os.O_APPEND | os .O_CREAT, int(permissions, 8)), 'a'):
                    self._stats_filepath = filepath
                    self._stats_file_permissions = permissions
            except Exception as e:
                error = "Could not open file: {}".format(e)
                logger.error(error)

        return error

    def update_stats_conf(self):
        self._stats_redis = None
        self._stats_filepath = None
        errors = []
        redis_conf = self._stats.get('redis', None)
        file_conf = self._stats.get('file', None)
        if redis_conf:
            error = self.try_connect_redis(redis_conf)
            if error:
                errors.append({"configuration": "report_stats", "error": error})
        if file_conf:
            error = self.try_open_file(file_conf)
            if error:
                errors.append({"configuration": "report_stats", "error": error})

        return errors

    def report_stats(self, services, reports_conf, cv):
        """
        Run stats reporting at regular intervals
        """

        self._stats = reports_conf
        self.update_stats_conf()

        with cv:
            while self._continue:
                cv.wait(self._stats.get('interval', 10))
                if not self._continue:
                    logger.debug("Reporter: stopping")
                    break
                stats = json.dumps(services.monitor_all())
                logger.debug("reporting stats: {}".format(stats))

                if self._stats_redis:
                    try:
                        redis_pub = self._stats['redis'].get('channel', None)
                        redis_list = self._stats['redis'].get('list', None)
                        if redis_pub:
                            logger.debug("Reporting stats on Redis channel {}".format(redis_pub))
                            self._stats_redis.publish(redis_pub, stats.encode("ascii"))
                        if redis_list:
                            logger.debug("Reporting stats on Redis list {}".format(redis_list))
                            self._stats_redis.rpush(redis_list, stats.encode("ascii"))
                    except redis.exceptions.ConnectionError as e:
                        logger.error("Could not report stats to Redis: {}".format(e))

                if self._stats_filepath:
                    try:
                        logger.debug("Reporting stats to file {}".format(self._stats_filepath))
                        os.umask(0)
                        with open(os.open(self._stats_filepath, os.O_WRONLY | os.O_APPEND | os.O_CREAT, int(self._stats_file_permissions, 8)), 'a') as file:
                            file.write(stats+'\n')
                    except Exception as e:
                        logger.error("Could not write stats to file: {}".format(e))
