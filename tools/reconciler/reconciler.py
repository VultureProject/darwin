#!/usr/local/bin/python3
__author__ = "Theo BERTIN"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'Daemon to get back additional context from entries generating alerts'

import logging
import signal
import json
import sys
import os
import redis
from redis.exceptions import RedisError
from json import JSONDecodeError
from time import sleep
from threading import Thread, Event


alert_source = None
context_sources = []
alert_destinations = []


class FileManager():
	def __init__(self, filepath):
		self.filepath = filepath

		#Raises exception in case of error
		with open(self.filepath, 'a+') as file:
			logger.debug("File Manager: successfuly opened file '{}'".format(self.filepath))

	def __str__(self):
		return self.filepath

	def test_write(self):
		if os.path.exists(self.filepath):
			if os.path.isfile(self.filepath):
				#Check if file can be written (but don't write anything to it)
				return os.access(self.filepath, os.W_OK)
			else:
				#Path point ot a directory
				return False

		#File does not exist, only need to check if file can be created in directory
		directory = os.path.dirname(self.filepath)
		if not directory:
			directory = '.'
		return os.access(directory, os.W_OK)

	def add_alert(self, alert):
		try:
			with open(self.filepath, 'a') as logfile:
				logfile.write(json.dumps(alert) + '\n')
			return True
		except OSError as e:
			logger.error("File Manager: Error while writing alert to file '{}': {}".format(self.filepath, e))
			return False


class RedisManager:
	def __init__(self, ip=None, port=0, unix_socket=None, redis_list=None, redis_channel=None, max_tries=1, sec_between_retries=1, update_alert_handler=None):
		self.redis_list = redis_list
		self.redis_channel = redis_channel
		self.max_tries = max_tries
		self.sec_between_retries = sec_between_retries
		self.shutdown_flag = Event()

		if ip and port:
			self.redis = redis.Redis(host=ip, port=port, health_check_interval=20, decode_responses=True)
		elif unix_socket:
			self.redis = redis.Redis(unix_socket_path=unix_socket, health_check_interval=20, decode_responses=True)
		else:
			raise RuntimeError("Redis Manager: No valid parameters for Redis server, got ip='{}', port={}, socket='{}'".format(ip, port, unix_socket))

		if update_alert_handler is None:
			self.update_alert_handler = self._update_alert_handler
		else:
			self.update_alert_handler = update_alert_handler

		#Raises Exception in case of error
		self.redis.ping()
		logger.debug("Redis Manager: successfuly connected to Redis {}".format(self.redis))

	def __str__(self):
		return "{}, list '{}', channel '{}', max_retries '{}', sec_between_retries '{}'".format(self.redis, self.redis_list, self.redis_channel, self.max_tries, self.sec_between_retries)


	def test_write(self):
		try:
			self.redis.setex("test_key_value_write", 1, "value")
		except redis.ReadOnlyError:
			logger.error("Redis Manager: the redis server is a replica, cannot write")
			return False
		except redisError as e:
			logger.error("Redis Manager: write test failed -> {}".format(e))
			return False

		return True


	def get_context(self, evt_id):
		context = None
		try:
			context = self.redis.get(evt_id)
			if context:
				context = json.loads(context)
		except RedisError as e:
			logger.error("Redis Manager (context): error while getting context -> {}".format(e))
		except json.JSONDecodeError as e:
			logger.warning("Redis Manager (context): context is not a valid JSON -> {}".format(e))
			logger.debug("context: {}".format(context))
			context = {"msg": context}
		except (UnicodeDecodeError, AttributeError) as e:
			logger.error("Redis Manager (context): could not decode context properly -> {}".format(e))
			logger.debug("context: {}".format(context))

		return context


	def add_alert(self, alert):
		ret = 0
		alert_string = json.dumps(alert)
		if self.redis_channel:
			logger.info("Redis Manager: publishing alert to channel '{}'".format(self.redis_channel))
			try:
				self.redis.publish(self.redis_channel, alert_string)
				ret += 1
			except RedisError as e:
				logger.error("Redis Manager: error while publishing alert -> {}".format(e))
				logger.debug("Redis Manager: redis -> {}, channel -> {}".format(self.redis, self.redis_channel))
		if self.redis_list:
			logger.info("Redis Manager: adding alert to list '{}'".format(self.redis_list))
			try:
				self.redis.lpush(self.redis_list, alert_string)
				ret += 1
			except RedisError as e:
				logger.error("Redis Manager: error while pushing to list -> {}".format(e))
				logger.debug("Redis Manager: redis -> {}, list -> {}".format(self.redis, self.redis_list))

		return ret


	def _update_alert_handler(self, alert, context):
		alert["context"] = context

	def _reconcile_alert(self, alert, retry=True):
		global context_sources
		global alert_destinations
		retries = 0
		context = None

		evt_id = alert.get("evt_id", None)
		if not evt_id:
			logger.warning("Redis Manager: got an alert without 'evt_id', ignoring")
			return

		while (retries < self.max_tries) and (context is None) and (not self.shutdown_flag.is_set()):
			for context_source in context_sources:
				context = context_source.get_context(evt_id)
				if context is not None:
					break
			retries += 1
			if context is None:
				if retries < self.max_tries and retry:
					logger.debug("Redis Manager: context not found for id {}, "
					"waiting {} second(s) for retry {}/{}".format(evt_id, self.sec_between_retries, retries + 1, self.max_tries))
					sleep(self.sec_between_retries)
				else:
					logger.warning("Redis Manager: could not find context associated with evt_id '{}'".format(evt_id))

		if context is not None:
			self.update_alert_handler(alert, context)

		for destination in alert_destinations:
			destination.add_alert(alert)


	def _parse_alert(self, alert):
		parsed_alert = None
		try:
			# parsed_alert = alert.decode()
			parsed_alert = json.loads(alert)
		except (UnicodeDecodeError, AttributeError) as e:
			logger.error("Redis Manager: could not decode alert -> {}".format(e))
			logger.debug("Redis Manager: alert = '{}'".format(alert))
		except JSONDecodeError as e:
			logger.error("Redis Manager: alert is not a valid JSON -> {}".format(e))
			logger.debug("Redis Manager: alert = '{}'".format(alert))

		return parsed_alert


	def _pop_alerts(self, retry=True):
		logger.debug("Redis Manager: popping alerts...")
		try:
			alert = self.redis.rpop(self.redis_list)
		except RedisError as e:
			logger.error("Redis Manager: error while trying to get alert from redis -> {}".format(e))
			logger.debug("Redis Manager: redis is {}, list is {}".format(self.redis, self.redis_list))
			return

		while (alert is not None) and (not self.shutdown_flag.is_set()):
			logger.debug("Redis Manager: got alert -> {}, {}".format(alert, type(alert)))
			parsed_alert = self._parse_alert(alert)
			if parsed_alert is not None:
				self._reconcile_alert(parsed_alert, retry)

			try:
				alert = self.redis.rpop(self.redis_list)
			except RedisError as e:
				logger.error("Redis Manager: error while trying to get alert from redis -> {}".format(e))
				logger.debug("Redis Manager: redis is {}, list is {}".format(self.redis, self.redis_list))
				return
		logger.debug("Redis Manager: finished popping")

	def monitor(self):
		listener = None

		logger.info("Redis Manager started")
		if self.redis_channel:
			try:
				logger.info("Redis Manager: listening to channel '{}'".format(self.redis_channel))
				listener = self.redis.pubsub()
				listener.subscribe([self.redis_channel])
			except RedisError as e:
				logger.error("Redis Manager: Could not subscribe to redis channel: {}".format(e))
				logger.debug("Redis Manager: redis -> {}, channel -> {}".format(self.redis, self.redis_channel))
				return

			if self.redis_list:
				#Check list before waiting in channel
				self._pop_alerts(retry=False)

		while not self.shutdown_flag.is_set():
			if listener:
				try:
					alert = listener.get_message(ignore_subscribe_messages=True, timeout=2)
				except redis.ConnectionError:
					logger.warning("Redis Manager: disconnected from Redis, waiting {} seconds before retrying".format(self.sec_between_retries))
					sleep(self.sec_between_retries)
				# If we have no messages, alert is None
				if alert:
					logger.debug("Redis Manager: got '{}' from channel {}".format(alert, self.redis_channel))
					if self.redis_list:
						# Only use the channel to trigger popping alerts from list (more reliable)
						self._pop_alerts()
					else:
						alert = alert.get("data", "")
						parsed_alert = self._parse_alert(alert)
						if parsed_alert is not None:
							self._reconcile_alert(parsed_alert)
			else:
				#Poll alerts list every n seconds
				self._pop_alerts()
				# Sleep DELAY time, 2 seconds at a time to prevent long sleep when shutdown_flag is set
				counter = 0
				while not self.shutdown_flag.is_set() and counter < self.sec_between_retries:
					sleep(2)
					counter += 2

		logger.info("Redis Manager stopped")


	def stop(self):
		logger.info("Redis Manager stopping")
		self.shutdown_flag.set()



def sig_hdlr(sigin, frame):
	global alert_source
	alert_source.stop()


def configure_logging(verbosity=0, logfile=None, custom_logger=None):
	global logger

	if custom_logger is not None:
		logger = custom_logger
	else:
		logger = logging.getLogger()
		file_handler = None
		stdout_handler = logging.StreamHandler(sys.stdout)

		# Logger config
		if verbosity == 0:
			loglevel = logging.ERROR
			print("verbosity is ERROR")
		elif verbosity == 1:
			loglevel = logging.WARNING
			print("verbosity is WARNING")
		elif verbosity == 2:
			loglevel = logging.INFO
			print("verbosity is INFO")
		else:
			loglevel = logging.DEBUG
			print("verbosity is DEBUG")

		# Add caller function name in logs when in debug
		if verbosity > 2:
			file_formatter = logging.Formatter(
				'{"date":"%(asctime)s","level":"%(levelname)s","function":"%(funcName)s()","message":"%(message)s"}')

			stdout_formatter = logging.Formatter(
				'%(asctime)s [%(levelname)s] - %(funcName)s() - %(message)s')
		else:
			file_formatter = logging.Formatter(
				'{"date":"%(asctime)s","level":"%(levelname)s","message":"%(message)s"}')

			stdout_formatter = logging.Formatter(
				'%(asctime)s [%(levelname)s] %(message)s')

		if logfile:
			file_handler = logging.FileHandler(logfile)
			file_handler.setLevel(loglevel)
			file_handler.setFormatter(file_formatter)
			logger.addHandler(file_handler)

		stdout_handler.setLevel(loglevel)
		stdout_handler.setFormatter(stdout_formatter)

		logger.addHandler(stdout_handler)
		logger.setLevel(loglevel)


conf_schema = {
	"$schema": "http://json-schema.org/draft-07/schema#",
	"definitions": {
		"redis_server":{
			"type": "object",
			"properties": {
				"unix": {"type": "string"},
				"ip": {"type": "string"},
				"port": {"type": "number"},
				"list": {"type": "string"},
				"channel": {"type": "string"}
			},
			"anyOf": [
				{"required": ["ip", "port"]},
				{"required": ["unix"]}
			]
		},
		"file": {
			"type": "object",
			"properties": {
				"path": {"type":"string"}
			},
			"required": ["path"]
		}
	},
	"properties": {
		"alerts_source": {
			"type": "object",
			"properties": {
				"redis": {
					"allOf": [
						{"$ref": "#/definitions/redis_server"},
						{
							"properties": {
								"seconds_between_retries": {
									"type": "number",
									"minimum": 1
									},
								"max_retries": {
									"type": "number",
									"minimum": 1
									},
							}
						}
					]
				},
			},
			"required": ["redis"],
			"additionalProperties": False
		},
		"context_source": {
			"type": "object",
			"properties": {
				"redis": {"$ref": "#/definitions/redis_server"},
			},
			"required": ["redis"],
			"additionalProperties": False
		},
		"alerts_destination": {
			"type": "object",
			"properties": {
				"redis": {"$ref": "#/definitions/redis_server"},
				"file": {"$ref": "#/definitions/file"}
			},
			"minProperties": 1,
			"additionalProperties": False
		},
		"required": ["alerts_source", "context_source", "alerts_destination"],
		"additionalProperties": False
	}
}


def parse_conf(conf_file=""):
	configuration = None
	try:
		with open(conf_file, 'r') as f:
			logger.debug("Loading config file")
			configuration = json.load(f)
	except Exception as e:
		logger.critical("Unable to load configuration: {}".format(e))
		raise e

	try:
		jsonschema.Draft7Validator(conf_schema).validate(configuration)
	except jsonschema.exceptions.ValidationError as e:
		logger.error("Incorrect configuration format: {}".format(e.message))
		raise e
	except jsonschema.exceptions.SchemaError as e:
		logger.error("Incorrect validation schema: {}".format(e.message))
		raise e

	return configuration


if __name__ == '__main__':
	import jsonschema
	import argparse

	signal.signal(signal.SIGINT, sig_hdlr)
	signal.signal(signal.SIGTERM, sig_hdlr)

	# Argparse
	parser = argparse.ArgumentParser(
		description="""This tools allows to monitor alerts generated by Darwin,
		recover context data through an unique ID
		and generate a new contextualized alert""")
	parser.add_argument('config_file', type=str, help='The configuration file to use')
	parser.add_argument('-v', '--verbose', action="count", default=0, help="activate verbose output ('v' is WARNING, 'vv' is INFO, 'vvv' is DEBUG)")
	parser.add_argument('-l', '--log-file', metavar="logfile", type=str, help="path to file to write logs to (default is only stdout)")
	args = parser.parse_args()

	configure_logging(args.verbose, args.log_file)

	try:
		configuration = parse_conf(args.config_file)
	except jsonschema.exceptions.ValidationError or jsonschema.exceptions.SchemaError:
		logger.error("Could not load configuration file")
		sys.exit(1)

	try:
		alert_source = RedisManager(
			configuration["alerts_source"]["redis"].get("ip", None),
			configuration["alerts_source"]["redis"].get("port", 0),
			configuration["alerts_source"]["redis"].get("unix", None),
			configuration["alerts_source"]["redis"].get("list", None),
			configuration["alerts_source"]["redis"].get("channel", None),
			configuration["alerts_source"]["redis"].get("max_retries", 3),
			configuration["alerts_source"]["redis"].get("seconds_between_retries", 1))
	except Exception as e:
		logger.error("Could not initialize alert source: {}".format(e))
		sys.exit(1)

	try:
		redis_context_source = RedisManager(
			configuration["context_source"]["redis"].get("ip", None),
			configuration["context_source"]["redis"].get("port", 0),
			configuration["context_source"]["redis"].get("unix", None)
		)
		context_sources.append(redis_context_source)
	except Exception as e:
		logger.error("Could not initialize context source: {}".format(e))
		sys.exit(1)

	try:
		if configuration["alerts_destination"].get("redis", None):
			redis_alert_dest = RedisManager(
				configuration["alerts_destination"]["redis"].get("ip", None),
				configuration["alerts_destination"]["redis"].get("port", 0),
				configuration["alerts_destination"]["redis"].get("unix", None),
				configuration["alerts_destination"]["redis"].get("list", None),
				configuration["alerts_destination"]["redis"].get("channel", None))
			alert_destinations.append(redis_alert_dest)

		if configuration["alerts_destination"].get("file", None):
			file_alert_dest = FileManager(
				configuration["alerts_destination"]["file"].get("path", None))
			alert_destinations.append(file_alert_dest)
	except Exception as e:
		logger.error("Could not initialize alert destination(s): {}".format(e))
		sys.exit(1)

	for alert_destination in alert_destinations:
		if not alert_destination.test_write():
			logger.error("Cannot write alerts to {}".format(alert_destination))
			sys.exit(1)

	if alert_source.redis_list is not None:
		if not alert_source.test_write():
			logger.error("Need write rights to 'alert_source' when a 'list' is specified")
			sys.exit(1)

	#Blocking loop to get alerts from Redis
	logger.info("starting Monitor")
	alert_source.monitor()
	logger.info("Monitor stopped")
