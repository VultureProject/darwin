#!/usr/local/bin/python3
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
from threading import Thread, Condition
from time import sleep
from config import load_conf, ConfParseError
from config import filters as conf_filters
from config import stats_reporting as conf_stats_report

# Argparse
parser = argparse.ArgumentParser()
parser.add_argument('config_file', type=str, help='The configuration file to use')
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
    try:
        load_conf(args.config_file)
    except ConfParseError as e:
        logger.critical(e)
        exit(1)

    services = Services(conf_filters)
    try:
        logger.info("Starting services...")
        services.start_all()
    except Exception as e:
        logger.error("Could not start all the services: {0}; exiting".format(e))
        services.stop_all()
        exit(1)

    stopCond = Condition()
    reporterThread = Thread(target=server.report_stats, args=(services, conf_stats_report, stopCond))
    logger.info("launching Reporter thread...")
    reporterThread.start()

    heartbeatThread = Thread(target=server.constant_heartbeat, args=(services, stopCond))
    logger.info("launching HeartBeat thread...")
    heartbeatThread.start()

    logger.info("Running server...")
    server.run(services)

    # Received Stop signal
    logger.info("Stopping services...")
    with stopCond:
        logger.debug("notifying threads to stop...")
        stopCond.notifyAll()
    logger.debug("Joining Heartbeat")
    heartbeatThread.join(1)
    try:
        logger.debug("Joining Reporter")
        reporterThread.join(1)
    except RuntimeError:
        logger.debug("Reporter not started, ignoring")
    logger.debug("Stop all")
    services.stop_all()
    logger.debug("Clean all")
    sleep(2)
    services.clean_all()
    logger.info("Ended successfully")
