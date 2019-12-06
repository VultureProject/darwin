from conf import DEFAULT_FILTER_PATH
from string import Template
import json
import logging
from time import sleep
from tools.redis_utils import RedisServer
import os
import stat

from manager_socket.utils import requests, PATH_CONF_FLOGS, CONF_ONE, CONF_FLOGS, REQ_MONITOR, REQ_MONITOR_CUSTOM_STATS
from tools.darwin_utils import darwin_configure, darwin_remove_configuration, darwin_start, darwin_stop
from tools.output import print_result

CONF_TEMPLATE = Template("""{
    "version": 2,
    "filters": [
        {
            "name": "logs_1",
            "exec_path": "${log_path}darwin_logs",
            "config_file": "${conf_path}",
            "output": "NONE",
            "next_filter": "",
            "nb_thread": 1,
            "log_level": "DEBUG",
            "cache_size": 0
        }
    ],
    "report_stats": {
        ${conf_redis}
        ${conf_file}
        ${proc_stats}
        "interval": 1
    }
}""")

DEFAULT_REDIS_SOCKET = "/tmp/darwin_test_redis.sock"
DEFAULT_REDIS_CHANNEL = "darwin.tests"
DEFAULT_REDIS_LIST = "darwin_tests"
DEFAULT_STATS_FILE = "/tmp/darwin_stats_test.log"
STAT_LOG_MATCH = '{"logs_1": {"status": "running", "connections": 0, "received": 0, "entryErrors": 0, "matches": 0, "failures": 0, "proc_stats": {"'


def run():
    tests = [
        proc_stats_default,
        proc_stats_other_defaults,
        proc_stats_custom,
        redis_reports,
        file_reports,
        file_and_redis_simple_report,
    ]

    for i in tests:
        print_result("Stats reporting: " + i.__name__, i)


def proc_stats_default():

    ret = False

    darwin_configure(CONF_TEMPLATE.substitute(log_path=DEFAULT_FILTER_PATH, conf_path=PATH_CONF_FLOGS, conf_redis="", conf_file="", proc_stats=""))
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    try:
        resp = json.loads(requests(REQ_MONITOR))
        resp = json.loads(resp)

        try:
            result = resp['logs_1']['proc_stats']
            if [x in result for x in ['memory_percent', 'cpu_percent']] and len(result) == 2:
                ret = True
            else:
                logging.error("proc_stats_default(): not expected result -> {}".format(result))
        except KeyError as e:
            logging.error("proc_stats_default(): {}".format(e))

    except Exception as e:
        logging.error("proc_stats_default(): error loading json -> {}".format(e))

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def proc_stats_other_defaults():

    ret = False

    darwin_configure(CONF_TEMPLATE.substitute(log_path=DEFAULT_FILTER_PATH, conf_path=PATH_CONF_FLOGS, conf_redis="", conf_file="", proc_stats='"proc_stats": ["name", "username", "memory_full_info"],'))
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    try:
        resp = json.loads(requests(REQ_MONITOR))
        resp = json.loads(resp)

        try:
            result = resp['logs_1']['proc_stats']
            if [x in result for x in ['name', 'username', 'memory_full_info']] and len(result) == 3:
                ret = True
            else:
                logging.error("proc_stats_other_defaults(): not expected result -> {}".format(result))
        except KeyError as e:
            logging.error("proc_stats_other_defaults(): {}".format(e))

    except Exception as e:
        logging.error("proc_stats_other_defaults(): {}".format(e))

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def proc_stats_custom():

    ret = False

    darwin_configure(CONF_TEMPLATE.substitute(log_path=DEFAULT_FILTER_PATH, conf_path=PATH_CONF_FLOGS, conf_redis="", conf_file="", proc_stats=""))
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)
    process = darwin_start()

    try:
        resp = json.loads(requests(REQ_MONITOR_CUSTOM_STATS))
        resp = json.loads(resp)

        try:
            result = resp['logs_1']['proc_stats']
            if [x in result for x in ['name', 'pid', 'memory_percent']] and len(result) == 3:
                ret = True
            else:
                logging.error("proc_stats_custom(): not expected result -> {}".format(result))
        except KeyError as e:
            logging.error("proc_stats_custom(): {}".format(e))

    except Exception as e:
        logging.error("proc_stats_custom(): {}".format(e))

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)
    return ret


def redis_reports():
    ret = False
    part = 1

    connections = [
        {"name": "ip", "conf": "\"ip\": \"127.0.0.1\", \"port\": 7890", "partial_result": ""},
        {"name": "ip_wrong_port", "conf": "\"ip\": \"127.0.0.1\", \"port\": 7891", "partial_result": "nodata"},
        {"name": "ip_wrong_ip", "conf": "\"ip\": \"127.0.0.2\", \"port\": 7890", "partial_result": "nodata"},
        {"name": "unix", "conf": "\"unix_path\": \"{}\"".format(DEFAULT_REDIS_SOCKET), "partial_result": ""},
        {"name": "unix_wrong", "conf": "\"unix_path\": \"wrong_path\"", "partial_result": "nodata"}
    ]

    report_methods = [
        {"name": "list", "conf": "\"list\": \"{}\"".format(DEFAULT_REDIS_LIST), "partial_result": STAT_LOG_MATCH},
        {"name": "channel", "conf": "\"channel\": \"{}\"".format(DEFAULT_REDIS_CHANNEL), "partial_result": STAT_LOG_MATCH},
    ]

    for method in report_methods:
        for connection in connections:
            print("[{}/10]".format(part), end='', flush=True)
            redis_conf = "\"redis\": {{{connection}, {method}}},".format(connection=connection['conf'], method=method['conf'])
            expected_result = method['partial_result'] if connection['partial_result'] == "" else connection['partial_result']

            darwin_configure(CONF_TEMPLATE.substitute(log_path=DEFAULT_FILTER_PATH, conf_path=PATH_CONF_FLOGS,
                conf_redis=redis_conf, conf_file="", proc_stats=""))
            darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)

            redis_server = RedisServer(unix_socket=DEFAULT_REDIS_SOCKET, address="127.0.0.1", port=7890)
            redis_probe = redis_server.connect()

            process = darwin_start()

            if method['name'] == "channel":
                redis_pubsub = redis_probe.pubsub(ignore_subscribe_messages=True)
                redis_pubsub.subscribe(DEFAULT_REDIS_CHANNEL)
                # Remove subscribe notification message
                sleep(0.1)
                redis_pubsub.get_message()
                sleep(1)
                message = redis_pubsub.get_message()

                try:
                    message = str(message['data'])
                except Exception:
                    message = "nodata"
            elif method['name'] == "list":
                sleep(1)
                message = redis_probe.lpop(DEFAULT_REDIS_LIST)
                if not message:
                    message = "nodata"
                else:
                    message = str(message)

            ret = True if expected_result in message else False
            if not ret:
                logging.error("redis_reports(): while trying '{}' with '{}', didn't find '{}' in '{}'".format(method['conf'], connection['conf'], expected_result, message))

            del redis_server
            del message
            darwin_stop(process)
            darwin_remove_configuration()
            darwin_remove_configuration(path=PATH_CONF_FLOGS)
            # Don't judge me...
            part += 1
            print("\x08\x08\x08\x08\x08\x08", end='', flush=True)
            if not ret:
                break
        if not ret:
            break

    # Dirty much... But pretty printing!
    if part == 11:
        print("\x08", end='', flush=True)
    print("       ", end='', flush=True)
    print("\x08\x08\x08\x08\x08\x08\x08", end='', flush=True)

    return ret


def file_reports():
    ret = False
    part = 1

    files = [
        {"name": "good-file", "conf": "\"filepath\": \"{}\"".format(DEFAULT_STATS_FILE), "result": True},
        {"name": "wrong-file", "conf": "\"filepath\": \"/tmp/but/I/do/not/exist\"", "result": False}
    ]

    permissions = [
        {"name": "default", "conf": "", "result": True, "expected_mode": "-rw-r-----"},
        {"name": "custom-correct", "conf": ",\"permissions\": 700", "result": True, "expected_mode": "-rwx------"},
        {"name": "custom-wrong", "conf": ",\"permissions\": -66", "result": False, "expected_mode": ""},
    ]

    for logfile in files:
        for permission in permissions:
            print("[{}/6]".format(part), end='', flush=True)
            file_conf = "\"file\": {{{file} {permission}}},".format(file=logfile['conf'], permission=permission['conf'])
            # print(file_conf)

            if logfile['result'] and permission['result']:
                expected_result = STAT_LOG_MATCH
            else:
                expected_result = "nothing"

            darwin_configure(CONF_TEMPLATE.substitute(log_path=DEFAULT_FILTER_PATH, conf_path=PATH_CONF_FLOGS,
                conf_redis="", conf_file=file_conf, proc_stats=""))
            darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)

            process = darwin_start()

            logline = "nothing"

            try:
                with open(DEFAULT_STATS_FILE, "r") as test_file:
                    file_stats = os.stat(DEFAULT_STATS_FILE)
                    mode = stat.filemode(file_stats.st_mode)
                    if mode != permission['expected_mode']:
                        logging.error("file_reports(): file mode should be '{}', but is '{}'".format(permission['expected_mode'], mode))
                        ret = False
                        # break
                    else:
                        sleep(1)
                        logline = test_file.readline()

                        ret = True if expected_result in logline else False
                        if not ret:
                            logging.error("file_reports(): while trying '{}' with '{}', didn't find '{}' in '{}'".format(logfile['conf'], permission['conf'], expected_result, logline))
            except FileNotFoundError:
                if expected_result != "nothing":
                    logging.error("file_reports(): file is absent, but should have been created")
                    ret = False

            try:
                os.remove(DEFAULT_STATS_FILE)
            except Exception:
                pass

            darwin_stop(process)
            darwin_remove_configuration()
            darwin_remove_configuration(path=PATH_CONF_FLOGS)
            # Don't judge me...
            part += 1
            print("\x08\x08\x08\x08\x08", end='', flush=True)
            if not ret:
                break
        if not ret:
            break

    # Dirty much... But pretty printing!
    print("      ", end='', flush=True)
    print("\x08\x08\x08\x08\x08\x08", end='', flush=True)

    return ret


def file_and_redis_simple_report():
    ret = False

    file_conf = """"file": {{"filepath": "{}"}},""".format(DEFAULT_STATS_FILE)
    redis_conf = """"redis": {{"ip": "127.0.0.1", "port": 7890, "channel": "{}", "list": "{}"}},""".format(DEFAULT_REDIS_CHANNEL, DEFAULT_REDIS_LIST)


    darwin_configure(CONF_TEMPLATE.substitute(log_path=DEFAULT_FILTER_PATH, conf_path=PATH_CONF_FLOGS,
        conf_redis=redis_conf, conf_file=file_conf, proc_stats=""))
    darwin_configure(CONF_FLOGS, path=PATH_CONF_FLOGS)

    redis_server = RedisServer(address="127.0.0.1", port=7890)
    redis_probe = redis_server.connect()
    redis_pubsub = redis_probe.pubsub(ignore_subscribe_messages=True)
    redis_pubsub.subscribe(DEFAULT_REDIS_CHANNEL)
    # Remove subscribe notification message
    sleep(0.1)
    redis_pubsub.get_message()

    process = darwin_start()

    try:
        with open(DEFAULT_STATS_FILE, "r") as statsfile:
            sleep(1)
            channel_message = redis_pubsub.get_message().get('data').decode("utf-8")
            list_message = redis_probe.lpop(DEFAULT_REDIS_LIST).decode("utf-8")
            file_message = statsfile.readline()
            # File message has an additional '\n'
            if channel_message in file_message and list_message in file_message:
                if STAT_LOG_MATCH in file_message:
                    ret = True
                else:
                    logging.error("file_and_redis_simple_report(): expected \"{}\", but got \"{}\"".format(STAT_LOG_MATCH, file_message))
            else:
                logging.error("file_and_redis_simple_report(): messages are not identical")
                logging.error("file_and_redis_simple_report(): {}".format(channel_message))
                logging.error("file_and_redis_simple_report(): {}".format(list_message))
                logging.error("file_and_redis_simple_report(): {}".format(file_message))
    except Exception as e:
        logging.error("file_and_redis_simple_report(): could not open file -> {}".format(e))

    try:
        os.remove(DEFAULT_STATS_FILE)
    except Exception:
        pass

    darwin_stop(process)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FLOGS)

    return ret