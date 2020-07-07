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
import os
from sys import exit
from Services import Services
from logging import FileHandler
from Administration import Server
from threading import Thread, Condition
from time import sleep
from config import load_conf, ConfParseError
from config import filters as conf_filters
from config import stats_reporting as conf_stats_report

def create_dirs(dirs, prefix, suffix):
    """
        Create directories if they don't exist
        :param dirs: name of the directories to be created
        :param prefix: directory's path preffix
        :param suffix : directory's path suffix
    """
    for d in dirs:
        path = '{}/{}{}'.format(prefix, d, suffix)
        if not os.path.exists(path):
            os.mkdir(path)

# Argparse
parser = argparse.ArgumentParser()
parser.add_argument('config_file', type=str, help='The configuration file to use')
parser.add_argument('-l', '--log-level',
                    help='Set log level to DEBUG, INFO, WARNING (default), ERROR or CRITICAL',
                    choices=['DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL'],
                    type=str)
parser.add_argument('-p', '--prefix-directories',
                    help='Set the prefix used for darwin files, default : \"/var\"',
                    default='/var',
                    type=str)

suffix_exclusive = parser.add_mutually_exclusive_group()
suffix_exclusive.add_argument('-s', '--suffix-directories',
                    help='Set the suffix used for darwin files, default : \"/darwin\"',
                    default='/darwin',
                    type=str)
suffix_exclusive.add_argument('--no-suffix-directories',
                    help='Use no suffix for darwin files',
                    action='store_true')

args = parser.parse_args()

prefix = '{}'.format(args.prefix_directories)
if args.no_suffix_directories:
    suffix = ''
else:
    suffix = '{}'.format(args.suffix_directories)

prefix = "/" + prefix.strip("/")
suffix = "/" + suffix.strip("/")
create_dirs(['log', 'run', 'sockets'], prefix, suffix)

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

# Create log file if doesn't exist
log_path = '{}/log{}/darwin_manager.log'.format(prefix, suffix)
if not os.path.isfile(log_path):
    open(log_path, "a+").close()

file_handler = FileHandler(log_path, mode="a+")

file_handler.setLevel(loglevel)
file_handler.setFormatter(formatter)
logger.addHandler(file_handler)


def sig_hdlr(sigin, frame):
    logger.info("Caught signal {}: exiting...".format(sigin))
    if server and server.running:
        logger.debug("Stop Server")
        server.stop()
    elif services:
        logger.debug("Stop Services")
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
    logger.info("prefix path is '{}'".format(prefix))
    logger.info("suffix path is '{}'".format(suffix))

    server = Server(prefix, suffix)
    logger.info("Configuring...")
    try:
        load_conf(prefix, suffix, args.config_file)
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
