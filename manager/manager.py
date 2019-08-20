#!/usr/local/bin/python3
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
__doc__ = 'Entry point of the program'

import logging
import argparse
import signal
import atexit
from sys import exit
from daemon import DaemonContext
from lockfile import FileLock
from Services import Services
from logging import FileHandler
from Administration import Server
from threading import Thread
from time import sleep

# Argparse
parser = argparse.ArgumentParser()
parser.add_argument('config_file', type=str, help='The config file to use')
parser.add_argument('-l', '--log-level',
                    help='Set log level to DEBUG, INFO, WARNING (default), ERROR or CRITICAL',
                    choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                    type=str)
args = parser.parse_args()

# Logger config
loglevel = logging.WARNING
if args.log_level == 'DEBUG':
    loglevel = logging.DEBUG
elif args.log_level == 'INFO':
    loglevel = logging.INFO
elif args.log_level == 'ERROR':
    loglevel = logging.ERROR
elif args.log_level == 'CRITICAL':
    loglevel = logging.CRITICAL

logger = logging.getLogger()
logger.setLevel(loglevel)

formatter = logging.Formatter(
    '{"date":"%(asctime)s","level":"%(levelname)s","message":"%(message)s"}')

file_handler = FileHandler('/var/log/darwin/darwin_manager.log')

file_handler.setLevel(loglevel)
file_handler.setFormatter(formatter)
logger.addHandler(file_handler)


def sig_hdlr(sigin, frame):
    logger.info("Caught signal {}: exiting...".format(sigin))
    if server and server.running:
        server.stop()
    elif services:
        services.stop_all()
        exit(0)
    else:
        exit(0)


def rotate_logs(sigin, frame):
    logger.info("Caught signal {}: rotating logs...".format(sigin))
    if services:
        services.rotate_logs_all()
    else:
        logger.info("No service to rotate logs with")


if __name__ == '__main__':

    signal.signal(signal.SIGINT, sig_hdlr)
    signal.signal(signal.SIGTERM, sig_hdlr)
    signal.signal(signal.SIGUSR1, rotate_logs)
    signal.signal(signal.SIGHUP, rotate_logs)

    logger.info("Starting...")
    daemon_context = DaemonContext(pidfile=FileLock('/var/run/darwin/manager.pid'),)
    daemon_context.detach_process = False
    logger.debug("daemon DONE")

    server = Server()
    logger.info("Configuring...")
    services = Services(args.config_file)

    try:
        logger.info("Starting services...")
        services.start_all()
    except Exception as e:
        logger.error("Could not start all the services: {0}; exiting".format(e))
        services.stop_all()
        exit(1)

    t = Thread(target=server.constant_heartbeat, args=(services,))
    logger.info("Running server...")
    server.run(services)
    t.start()

    logger.info("Stopping services...")
    logger.debug("Joining...")
    t.join(1)
    if t.is_alive():
        logger.error("HeartBeat thread not responding... Ignoring & stopping filters")
    logger.debug("Stop all")
    services.stop_all()
    logger.debug("Clean all")
    sleep(2)
    services.clean_all()
    logger.info("Ended successfully")
