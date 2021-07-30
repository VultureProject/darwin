import functools
from manager_socket.utils import CONF_FTEST, PATH_CONF_FTEST
import time
from tools.output import print_result
from tools.darwin_utils import darwin_configure, darwin_stop, darwin_start, darwin_remove_configuration
from tools.filter import Filter
import conf
import psutil

def run():
    scenarios = [
        ['unix', 'unix', 'unix'],
        ['tcp', 'tcp', 'tcp'],
        ['udp', 'udp', 'udp'],
        ['udp', 'unix', 'unix'],
        ['tcp', 'unix', 'unix'],
        ['udp', 'tcp', 'tcp'],
        ['tcp', 'udp', 'unix'],
        ['unix', 'tcp', 'udp'],
    ]

    for i in scenarios:
        print_result("Multi Filters: Alerting tests : " + ' -> '.join(i), functools.partial(alerting_tests, i))

ONE_FILTER = """
{{
        "name": "{name}",
        "exec_path": "{filter_path}darwin_test",
        "config_file": "/tmp/test.conf",
        "output": "RAW",
        {next_filter}
        "nb_thread": 1,
        "log_level": "DEBUG",
        "cache_size": 0,
        "network":{network}
    }}
"""
UNIX_NETWORK = """{{
    "socket_type":"UNIX"
}}"""

TCP_NETWORK = """{{
    "socket_type":"TCP",
    "address_path":"[::1]:{port}"
    }}"""
UDP_NETWORK = """{{
    "socket_type":"UDP",
    "address_path":"[::1]:{port}"
    }}"""
network_map = {
    'unix': UNIX_NETWORK,
    'tcp': TCP_NETWORK,
    'udp': UDP_NETWORK
}
CONFIG = """
{{
    "version": 2,
    "filters": [
    {filter_1},
    {filter_2},
    {filter_3}
    ],
    "report_stats": {{
        "file": {{
            "filepath": "/tmp/darwin-stats",
            "permissions": 640
        }},
        "interval": 5
    }}
}}
"""

# Test different combinations of tcp/udp/unix socket and verifies that everytime, 3 alerts are spawned
def alerting_tests(filters_list):
    assert(len(filters_list) == 3)
    filter_1 = ONE_FILTER.format(name='test_1', filter_path=conf.DEFAULT_FILTER_PATH, 
                            next_filter='"next_filter": "test_2",', 
                            network=network_map[filters_list[0]].format(port=8181))
    filter_2 = ONE_FILTER.format(name='test_2', filter_path=conf.DEFAULT_FILTER_PATH, 
                            next_filter='"next_filter": "test_3",', 
                            network=network_map[filters_list[1]].format(port=8282))
    filter_3 = ONE_FILTER.format(name='test_3', filter_path=conf.DEFAULT_FILTER_PATH, 
                            next_filter='', 
                            network=network_map[filters_list[2]].format(port=8383))
    config = CONFIG.format(filter_1=filter_1, filter_2=filter_2, filter_3=filter_3)

    darwin_configure(config)
    darwin_configure(CONF_FTEST, path=PATH_CONF_FTEST)

    # Erasing last alerts
    file = open("/tmp/test_test.log","w")
    file.truncate(0)
    file.close()

    p = darwin_start()

    # socket path is given according to how it is currently generated in the manager, 
    # this may have to change in the future
    path_addr = '{}/sockets/test_1.1.sock'.format(conf.TEST_FILES_DIR) if filters_list[0] == 'unix' else '[::1]:8181'
    f = Filter(socket_type=filters_list[0], socket_path=path_addr)
    api = f.get_darwin_api()

    start = time.time()
    nb_test_filters=0
    # We check that all 3 filters are running, on VM HardenedBSD, it may take some time
    while nb_test_filters < 3 and time.time() - start < 6:
        time.sleep(1)
        nb_test_filters=0
        for proc in psutil.process_iter():
            try:
                # Check if process name contains the given name string.
                if 'darwin_test' in proc.name().lower():
                    nb_test_filters += 1
            except (psutil.NoSuchProcess, psutil.AccessDenied, psutil.ZombieProcess):
                pass

    api.call("Hello", response_type="darwin")
    time.sleep(0.2)
    api.call("There", response_type="darwin")

    # arbitrary sleep length, should be enough on most systems
    # We verify that One alert per filter were written
    time.sleep(2)
    line1, line2, line3 = 0, 0, 0
    file = open("/tmp/test_test.log","r")
    for l in file.readlines():
        if 'test_1' in l:
            line1 += 1
        elif 'test_2' in l:
            line2 += 1
        elif 'test_3' in l:
            line3 += 1
    file.close()
    darwin_stop(p)
    darwin_remove_configuration()
    darwin_remove_configuration(path=PATH_CONF_FTEST)

    if line1 != 2 or line2 != 2 or line3 != 2:
        print('test failed: ', filters_list)
        print("test1 : {}, test2 : {}, test3 : {}".format(line1, line2, line3))
        return False
    return True
