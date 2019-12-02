__author__ = "Hugo SOSZYNSKI"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'The unix socket administration server class'

import socket
import logging
from os import unlink

from JsonSocket import JsonSocket
from time import sleep


logger = logging.getLogger()


class Server:
    """
    Manages administration connections for filter update and monitoring.
    """

    def __init__(self):
        """
        Constructor. Create the UNIX socket to listen on.
        """
        self._continue = True
        self.running = False
        self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._socket.bind('/var/sockets/darwin/darwin.sock')
        self._socket.listen(5)
        self._socket.settimeout(1)

    def __del__(self):
        """
        Close the socket.
        """
        self._socket.close()
        unlink('/var/sockets/darwin/darwin.sock')

    @staticmethod
    def process(services, cli, cmd):
        """
        Process the incoming instruction.

        :param services: The services manager.
        :param cli: The client JsonSocket
        :param cmd: The instruction sent by the client.
        """
        response = {}
        if cmd.get('type', None):
            if cmd['type'] == 'update_filters':
                errors = services.update(cmd.get('filters', []))
                if not errors:
                    response['status'] = 'OK'
                else:
                    response['status'] = 'KO'
                    response['errors'] = errors
            elif cmd['type'] == 'monitor':
            response = services.monitor_all()

        try:
            cli.send(response)
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

    def constant_heartbeat(self, services):
        """
        Run constant heartbeat in a thread.
        """

        while self._continue:
            sleep(1)
            services.hb_all()
