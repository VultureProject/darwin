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
from redis.exceptions import RedisError, ReadOnlyError
from json import JSONDecodeError
from time import sleep
from threading import Thread, Event


alert_source = None
context_sources = []
alert_destinations = []


class FileManager():
	"""
	A simple class to handle alert writes to a file

	Attributes
	----------
	- filepath: str
		the fullpath to the file

	Methods
	-------
	- test_write():
		test if the file can be written to/created
	- test_read():
		test if the file can be read
	- add_alert(alert):
		add an alert to the previously set file (defined with 'filepath')
	"""


	def __init__(self, filepath: str):
		"""
		Parameters
		----------
		- filepath: str
			the fullpath to the file to use (or create) when writing alerts
			file won't be overriden if existing, alerts will be appended
			file will be created if not existing (subdirs won't be created though)
		"""

		self.filepath = filepath


	def __str__(self) -> str:
		"""
		Returns
		-------
		str:
			the fullpath defined during __init__()
		"""

		return self.filepath


	def test_write(self) -> bool:
		"""
		Test if the path provided exists, is a file, and can be written to

		Returns
		-------
		True -> path exists, is a file and can be written to, or can at least be created in the directory
		False -> otherwise
		"""

		if os.path.exists(self.filepath):
			if os.path.isfile(self.filepath):
				#Check if file can be written (but don't write anything to it)
				return os.access(self.filepath, os.W_OK)
			else:
				return False

		#File does not exist, only need to check if file can be created in directory
		directory = os.path.dirname(self.filepath)
		if not directory:
			directory = '.'
		return os.access(directory, os.W_OK)


	def test_read(self) -> bool:
		"""
		Test if the path provided exists, is a file, and can be read

		Returns
		-------
		True -> path exists, is a file and can be read
		False -> otherwise
		"""

		if os.path.exists(self.filepath):
			if os.path.isfile(self.filepath):
				#Check if file can be read
				return os.access(self.filepath, os.R_OK)
			else:
				logger.error("path {} is a directory".format(self.filepath))
		else:
			logger.error("file {} doesn't exist".format(self.filepath))

		return False


	def add_alert(self, alert: str) -> bool:
		"""
		Add alert to the file

		Parameters
		----------
		alert: str
			the alert to add to the file (a newline will be added to the alert)

		Returns
		-------
		True -> line was added to the file
		False -> an error occured while writing to file
		"""

		try:
			with open(self.filepath, 'a+') as logfile:
				logfile.write(json.dumps(alert) + '\n')
			return True
		except OSError as e:
			logger.error("File Manager: Error while writing alert to file '{}': {}".format(self.filepath, e))
			return False


class RedisManager:
	"""
	A class to handle Redis listening, reading and writing of source alerts, context and reconciled alerts

	Attributes
	----------
	- redis: object
		the Redis object containing connection information
	- redis_list: str
		the name of the list to use in Redis
	- redis_channel: str
		the name of the pubsub channel to use in Redis
	- max_tries: number
		the number of times to try and get context from Redis
	- sec_between_retries: number
		the number of seconds to wait between retries
		also used between pollings when using only a redis_list, and during reconnection attempts when Redis disconnects

	Methods
	-------
	- test_write()
		test if redis is valid and is not a replica
	- test_read()
		test if redis is valid and can be read
	- discover_clustering()
		function to discover if the current redis connection is a master, and if it's a replica change to master
	- get_context(evt_id)
		search for the context in Redis
	- add_alert(alert)
		add alert to list and/or channel in Redis
	- monitor()
		blocking function to start monitoring alerts in list and/or channel from Redis
	- stop()
		ask to stop monitoring
	"""


	def __init__(self, ip=None, port=0, unix_socket=None, redis_list=None, redis_channel=None, max_tries=1, sec_between_retries=1, update_alert_handler=None):
		"""
		Parameters
		----------
		- ip: str (default None)
			the ip of the Redis host to connect to
		- port: number (default 0)
			the port of the Redis host to connect to
		- unix_socket: str (default None)
			the fullpath to the unix socket to connect to
		- redis_list: str (default None)
			the list to look at when monitoring alerts
		- redis_channel: str (default None)
			the channel to listen to when monitoring alerts
		- max_tries: number (default 1)
			the number of tries when fetching context from Redis
		- sec_between_retries: number (default 1)
			number of seconds between retries, used for:
				- context fetching
				- reconnection to Redis
				- intervals between polling in list-only scenario
		- _redis_fallbacks: list
			list of replicas during Redis clustering
		- update_alert_handler: function(alert: dict, context: dict)
			parameter to define a custom function when putting context in alert
			default behaviour is putting context in a key 'context' at the root of the alert
		"""

		self.redis_list = redis_list
		self.redis_channel = redis_channel
		self.max_tries = max_tries
		self.sec_between_retries = sec_between_retries
		self._redis_fallbacks = []
		self._shutdown_flag = Event()

		if ip and port:
			self.redis = redis.Redis(host=ip, port=port, health_check_interval=20, decode_responses=True)
		elif unix_socket:
			self.redis = redis.Redis(unix_socket_path=unix_socket, health_check_interval=20, decode_responses=True)

		if update_alert_handler is None:
			self.update_alert_handler = self._update_alert_handler
		else:
			self.update_alert_handler = update_alert_handler


	def __str__(self) -> str:
		"""
		Returns
		-------
		str:
			the redis object stringified, the list, the channel, max_tries, sec_between_retries
		"""

		return "{}, list '{}', channel '{}', max_tries '{}', sec_between_retries '{}'".format(self.redis, self.redis_list, self.redis_channel, self.max_tries, self.sec_between_retries)


	def test_write(self) -> bool:
		"""
		Test if the Redis server can be written to (is not a replica)

		Returns
		-------
		True -> the Redis server is valid, and can be written to
		False -> an error occured while trying to connect or write to the Redis server (see logs for details)
		"""

		try:
			self.redis.setex("test_key_value_write", 1, "value")
		except redis.ReadOnlyError:
			logger.error("Redis Manager: the redis server is a replica, cannot write")
			return False
		except RedisError as e:
			logger.error("Redis Manager: write test failed -> {}".format(e))
			return False

		return True


	def test_read(self) -> bool:
		"""
		Test if the Redis server can be read

		Returns
		-------
		True -> the Redis server is valid, and can be accessed
		False -> an error occured while trying to connect or read from the Redis server (see logs for details)
		"""

		try:
			self.redis.ping()
		except RedisError as e:
			logger.error("Redis Manager: read test failed -> {}".format(e))
			return False

		return True


	def discover_clustering(self) -> bool:
		"""
		Uses current redis connection, and potential previously discovered fallbacks,
		to search for the current master server in a cluster
		If a valid master is found:
		- the active redis connection is updated
		- the list of potential fallbacks is updated with the connected replicas

		Returns
		-------
		True -> if a new master is found
		False -> otherwise
		"""

		logger.info("Redis Manager: discovering cluster...")
		try:
			info = self.redis.info()

			role = info.get("role", None)
			if not role:
				logger.error("Redis Manager: could not get role from redis server")
				logger.debug("Redis is {}".format(self.redis))
				logger.debug("info() is {}".format(info))
				return False

			if role == "slave":
				logger.debug("server is a replica")
				redis_master = redis.Redis(
									host=info.get("master_host", None),
									port=info.get("master_port", 0),
									health_check_interval=20,
									decode_responses=True)
				try:
					logger.debug("trying server {}".format(redis_master))
					info = redis_master.info()
					# If program is here, command was successful so that should be a valid master
					self.redis = redis_master
					# Just connected to a redis master, so the current role is 'master'
					role = "master"
				except RedisError:
					logger.warning("Redis Manager: error while trying to connect to presumed master, ignoring")

			if role == "master":
				logger.debug("server is a master")
				logger.info("Redis Manager: connected to master redis {}".format(self.redis))
				number_of_replicas = info.get("connected_slaves", 0)
				logger.debug("number of replicas in cluster: {}".format(number_of_replicas))
				self._redis_fallbacks = []
				for index in range(0, number_of_replicas):
					# Replicas connection info are in keys of the form 'slave#'
					redis_replica = info.get("slave"+str(index), None)
					if redis_replica:
						self._redis_fallbacks.append(
							redis.Redis(
								host=redis_replica.get("ip", None),
								port=redis_replica.get("port", 0),
								health_check_interval=20,
								decode_responses=True))
						logger.debug("added a replica in the pool: {}".format(self._redis_fallbacks[-1]))

				return True

		except RedisError as e:
			logger.error("Redis Manager: could not get infos from redis server -> {}".format(e))
			logger.debug("Redis is {}".format(self.redis))

		return False


	def _reconnect_to_master(self) -> bool:
		"""
		Function used to try to find a new master during clustering, when a read only error occurs on Redis
		This function uses current active connection and known fallbacks (if any) to try and find the current master
		If a new master is found and a pubsub listener was defined, the listener is updated to point to the new server

		Returns
		-------
		True -> a new active master was found
		False -> otherwise
		"""

		logger.warning("Redis Manager: trying to reconnect to a Redis master...")

		servers = self._redis_fallbacks
		servers.append(self.redis)

		for connection in servers:
			logger.debug("Redis Manager: trying with {}".format(connection))
			try:
				info = connection.info()
				# Should always contain the key 'role', but better safe than sorry
				if info and info.get("role", None):
					self.redis = connection
					if self.discover_clustering():
						logger.warning("Redis Manager: reconnected to a valid master")
						if self.redis_channel:
							# Have to replace current listener, but that's shitty, any other idea ?
							self._listener = self.redis.pubsub()
							self._listener.subscribe([self.redis_channel])
						return True
			except RedisError:
				logger.debug("Redis instance {} did not reply, ignoring".format(connection))

		logger.warning("Redis Manager: could not reconnect to a valid master")
		return False


	def get_context(self, evt_id: str) -> dict:
		"""
		Tries to return the context from the Redis server
		Searches the key defined by evt_id, if the key is not found, will try again for 'max_tries'
		every 'sec_between_tries' second
		If the returned context is a simple string, will put the string in a dict with the key 'msg'
		example: context = "this is just a string" -> returns {"msg": "this is just a string"}

		Parameters
		----------
		- evt_id: str
			the key to search for in Redis, should represent the event_id given in an alert, but could be anything

		Returns
		-------
		context -> a dict representing what was stored in Redis under the given key
		None -> nothing was retrieved or an error occured during operation
		"""

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


	def add_alert(self, alert) -> int:
		"""
		Add the alert to Redis using the means defined (list and/or channel) during __init__()

		Parameters
		----------
		- alert: dict
			the dict representing the alert to put in the list/channel

		Returns
		-------
		number -> the number of successful writes
			2 if list and channel were defined, and both succeded
			1 if only list or channel were defined and succeeded, or an error occured on one of them if both were defined
			0 if all means (be it list and/or channel) failed
		"""

		ret = 0
		alert_string = json.dumps(alert)
		if self.redis_channel:
			logger.info("Redis Manager: publishing alert to channel '{}'".format(self.redis_channel))
			try:
				self.redis.publish(self.redis_channel, alert_string)
				ret += 1
			except ReadOnlyError:
				logger.warning("Redis Manager: could not publish alert to server, the server doesn't allow write operations")
				# If there are alternatives, and a new master is found amongst them
				if self._reconnect_to_master():
					try:
						self.redis.publish(self.redis_channel, alert_string)
						ret += 1
					except RedisError:
						logger.error("Redis Manager: error while publishing alert -> {}".format(e))
						logger.debug("Redis Manager: redis -> {}, channel -> {}".format(self.redis, self.redis_channel))
			except RedisError as e:
				logger.error("Redis Manager: error while publishing alert -> {}".format(e))
				logger.debug("Redis Manager: redis -> {}, channel -> {}".format(self.redis, self.redis_channel))
		if self.redis_list:
			logger.info("Redis Manager: adding alert to list '{}'".format(self.redis_list))
			try:
				self.redis.lpush(self.redis_list, alert_string)
				ret += 1
			except ReadOnlyError:
				logger.warning("Redis Manager: could not write alert to server list, the server doesn't allow write operations")
				if self._reconnect_to_master():
					try:
						self.redis.lpush(self.redis_list, alert_string)
						ret += 1
					except RedisError:
						logger.error("Redis Manager: error while pushing to list -> {}".format(e))
						logger.debug("Redis Manager: redis -> {}, list -> {}".format(self.redis, self.redis_list))
			except RedisError as e:
				logger.error("Redis Manager: error while pushing to list -> {}".format(e))
				logger.debug("Redis Manager: redis -> {}, list -> {}".format(self.redis, self.redis_list))

		return ret


	def _update_alert_handler(self, alert: dict, context: dict):
		"""
		Adds the context in the alert under the "context" key

		Parameters
		----------
		- alert: dict
			the alert received
		- context: dict
			the context recovered
		"""

		alert["context"] = context


	def _reconcile_alert(self, alert: dict, retry=True):
		"""
		From the alert, recovers the 'evt_id', then queries for the context from the global 'context_sources'
		Will try to get context from all the sources defined in the list, using get_context(), and will try 'max_tries'
		times every 'sec_between_tries' seconds
		If context is recovered successfuly, will call the update handler (either default or custom defined one)
		to add context to the alert
		Finally, will call add_alert() from every manager defined in the global alert_destinations list to add the alert here

		Parameter
		---------
		- alert: dict
			the alert recovered from the list/channel
		- retry: bool (default True)
			whether to honour 'max_tries' when context wasn't recovered successfuly from context_source(s)
		"""

		global context_sources
		global alert_destinations
		retries = 0
		context = None

		evt_id = alert.get("evt_id", None)
		if not evt_id:
			logger.warning("Redis Manager: got an alert without 'evt_id', ignoring")
			return

		# Continue as long as context wasn't recoved, retries are still possible, and program isn't asked to stop
		while (retries < self.max_tries) and (context is None) and (not self._shutdown_flag.is_set()):
			# Search context in every context_source in turn (no delay involved here)
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

		# Add alert to destinations whether context was successfuly added to the alert or not
		for destination in alert_destinations:
			destination.add_alert(alert)


	def _parse_alert(self, alert: str) -> dict:
		"""
		Helper function to parse alert from JSON to Python dict

		Parameter
		---------
		- alert: str
			the JOSN alert as a (decoded) string

		Returns
		-------
		dict -> a dict representing the JSON alert
		None -> if the string wasn't a valid JSON
		"""

		parsed_alert = None
		try:
			parsed_alert = json.loads(alert)
		except JSONDecodeError as e:
			logger.error("Redis Manager: alert is not a valid JSON -> {}".format(e))
			logger.debug("Redis Manager: alert = '{}'".format(alert))

		return parsed_alert


	def _pop_alerts(self, retry=True):
		"""
		Pop alerts from the configured list as long as shutdown is not asked and there are alerts in the Redis list
		Will call _reconcile_alert() for each alert recovered

		Parameters
		----------
		- retry: bool (default True)
			whether to honour the 'max_tries' parameter, passed down during context query
		"""

		logger.debug("Redis Manager: popping alerts...")
		alert = None
		try:
			alert = self.redis.rpop(self.redis_list)
		except ReadOnlyError:
			logger.warning("Redis Manager: could not pop alert, the server doesn't allow writing")
			if self._reconnect_to_master():
				try:
					alert = self.redis.rpop(self.redis_list)
				except RedisError as e:
					logger.error("Redis Manager: error while trying to get alert from redis -> {}".format(e))
					logger.debug("Redis Manager: redis is {}, list is {}".format(self.redis, self.redis_list))
		except RedisError as e:
			logger.error("Redis Manager: error while trying to get alert from redis -> {}".format(e))
			logger.debug("Redis Manager: redis is {}, list is {}".format(self.redis, self.redis_list))
			return

		# Pop alerts from list as long as there are alerts, and program wasn't asked to stop
		while (alert is not None) and (not self._shutdown_flag.is_set()):
			logger.debug("Redis Manager: got alert -> {}, {}".format(alert, type(alert)))
			parsed_alert = self._parse_alert(alert)
			if parsed_alert is not None:
				self._reconcile_alert(parsed_alert, retry)

			try:
				alert = self.redis.rpop(self.redis_list)
			except ReadOnlyError:
				logger.warning("Redis Manager: could not pop alert, the server doesn't allow writing")
				if self._reconnect_to_master():
					try:
						alert = self.redis.rpop(self.redis_list)
					except RedisError as e:
						logger.error("Redis Manager: error while trying to get alert from redis -> {}".format(e))
						logger.debug("Redis Manager: redis is {}, list is {}".format(self.redis, self.redis_list))
			except RedisError as e:
				logger.error("Redis Manager: error while trying to get alert from redis -> {}".format(e))
				logger.debug("Redis Manager: redis is {}, list is {}".format(self.redis, self.redis_list))
				return
		logger.debug("Redis Manager: finished popping")


	def monitor(self):
		"""
		A blocking function to start active monitoring of alerts in Redis
		If only a channel is set, will listen for incoming alerts in the channel, and will reconcile each alert taken from it
		If a channel AND a list are set, will only trigger popping alerts from list as soon as a message in the channel is received
			(messages from the channel are ignored and can be anything)
		If only a list is set, will trigger popping of alerts in list every 'sec_between_tries' seconds
		To stop monitoring, one should call the stop() function
			as such, one should run this function in a separate thread, or assign the stop() function to a signal
		"""

		self._listener = None

		logger.info("Redis Manager started")
		if self.redis_channel:
			try:
				logger.info("Redis Manager: listening to channel '{}'".format(self.redis_channel))
				self._listener = self.redis.pubsub()
				self._listener.subscribe([self.redis_channel])
			except RedisError as e:
				logger.error("Redis Manager: Could not subscribe to redis channel: {}".format(e))
				logger.debug("Redis Manager: redis -> {}, channel -> {}".format(self.redis, self.redis_channel))
				return

			if self.redis_list:
				#Check list before waiting in channel
				self._pop_alerts(retry=False)

		while not self._shutdown_flag.is_set():
			if self._listener:
				try:
					alert = self._listener.get_message(ignore_subscribe_messages=True, timeout=2)
				except redis.ConnectionError:
					logger.warning("Redis Manager: disconnected from Redis, waiting {} second(s) before retrying".format(self.sec_between_retries))
					sleep(self.sec_between_retries)
					if self._reconnect_to_master():
						logger.warning("Redis Manager: found new valid redis master, resuming")
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
				while not self._shutdown_flag.is_set() and counter < self.sec_between_retries:
					sleep(2)
					counter += 2

		logger.info("Redis Manager stopped")


	def stop(self):
		"""
		Function to ask interruption when execution is in monitor()
		Warning, returning from the monitor() function could take as long as 2 seconds
		"""

		logger.info("Redis Manager stopping")
		self._shutdown_flag.set()



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
		jsonschema.Draft7Validator(conf_schema).validate(configuration)
	except IOError as e:
		logger.error("Error while opening configuration file: {}".format(e))
	except JSONDecodeError as e:
		logger.error("Error while parsing json in configuration: {}".format(e))
	except jsonschema.exceptions.ValidationError as e:
		logger.error("Incorrect configuration format: {}".format(e.message))
		configuration = None
	except jsonschema.exceptions.SchemaError as e:
		logger.error("Incorrect validation schema: {}".format(e.message))
		configuration = None

	return configuration


if __name__ == '__main__':
	import jsonschema
	import argparse

	signal.signal(signal.SIGINT, sig_hdlr)
	signal.signal(signal.SIGTERM, sig_hdlr)

	# Argparse definitions
	parser = argparse.ArgumentParser(
		description="""This tools allows to monitor alerts generated by Darwin,
		recover context data through an unique ID
		and generate a new contextualized alert""")
	parser.add_argument('config_file', type=str, help='The configuration file to use')
	parser.add_argument('-v', '--verbose', action="count", default=0, help="activate verbose output ('v' is WARNING, 'vv' is INFO, 'vvv' is DEBUG)")
	parser.add_argument('-l', '--log-file', metavar="logfile", type=str, help="path to file to write logs to (default is only stdout)")
	parser.add_argument('-c', '--cluster', action="store_true", help="indicate that redis instances might be replicas, program should research masters")
	args = parser.parse_args()

	# Logging configuration
	configure_logging(args.verbose, args.log_file)

	# Configuration file parsing
	configuration = parse_conf(args.config_file)
	if configuration is None:
		logger.error("Could not load configuration")
		sys.exit(1)

	### Manager instances loading ###
	# Alert sources
	alert_source = RedisManager(
		configuration["alerts_source"]["redis"].get("ip", None),
		configuration["alerts_source"]["redis"].get("port", 0),
		configuration["alerts_source"]["redis"].get("unix", None),
		configuration["alerts_source"]["redis"].get("list", None),
		configuration["alerts_source"]["redis"].get("channel", None),
		configuration["alerts_source"]["redis"].get("max_retries", 3),
		configuration["alerts_source"]["redis"].get("seconds_between_retries", 1))

	if args.cluster and not alert_source.discover_clustering():
		logger.error("alerts_source: clustering argument is given, but no valid master was found")
		logger.debug("alert_source is {}".format(alert_source))
		sys.exit(1)

	# Context source
	redis_context_source = RedisManager(
		configuration["context_source"]["redis"].get("ip", None),
		configuration["context_source"]["redis"].get("port", 0),
		configuration["context_source"]["redis"].get("unix", None)
	)

	if args.cluster and not redis_context_source.discover_clustering():
		logger.error("context_source: redis clustering argument is given, but no valid master was found")
		logger.debug("context_source is {}".format(redis_context_source))
		sys.exit(1)
	else:
		context_sources.append(redis_context_source)


	# Alerts destination
	if configuration["alerts_destination"].get("redis", None):
		redis_alert_dest = RedisManager(
			configuration["alerts_destination"]["redis"].get("ip", None),
			configuration["alerts_destination"]["redis"].get("port", 0),
			configuration["alerts_destination"]["redis"].get("unix", None),
			configuration["alerts_destination"]["redis"].get("list", None),
			configuration["alerts_destination"]["redis"].get("channel", None))

		if args.cluster and not redis_alert_dest.discover_clustering():
			logger.error("alerts_destination: redis clustering argument is given, but no valid master was found")
			logger.debug("alert_destination is {}".format(redis_alert_dest))
			sys.exit(1)
		else:
			alert_destinations.append(redis_alert_dest)

	if configuration["alerts_destination"].get("file", None):
		file_alert_dest = FileManager(
			configuration["alerts_destination"]["file"].get("path", None))
		alert_destinations.append(file_alert_dest)


	# Manager validations
	for alert_destination in alert_destinations:
		if not alert_destination.test_write():
			logger.error("Cannot write alerts to {}".format(alert_destination))
			sys.exit(1)

	if alert_source.redis_list is not None:
		if not alert_source.test_write():
			logger.error("Need write rights to 'alert_source' when a 'list' is specified")
			sys.exit(1)
	else:
		if not alert_source.test_read():
			logger.error("Could not read from the configured alert source")
			logger.error("alert source is {}".format(alert_source))
			sys.exit(1)

	for context_source in context_sources:
		if not context_source.test_read():
			logger.error("Could not read from a configured context source")
			logger.error("context source is {}".format(context_source))
			sys.exit(1)


	#Blocking loop to get alerts from Redis
	logger.info("starting Monitor")
	alert_source.monitor()
	logger.info("Monitor stopped")
