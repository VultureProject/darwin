import logging

from tools.filter import Filter
from tools.output import print_result
from darwin import DarwinApi

class Connection(Filter):
    def __init__(self):
        super().__init__(filter_name="connection")
        self.init_data_path = "/tmp/init_data_path.txt".format(self.filter_name)

    def write_conf(self):
        content = '{{\n' \
                  '"redis_socket_path": "/var/sockets/redis/redis.sock",\n' \
                  '"init_data_path": "{init_data_path}",\n' \
                  '"redis_expire": 300\n' \
                  '}}'.format(init_data_path=self.init_data_path)
        with open(self.config, mode='w') as file:
            file.write(content)

    def write_init_data(self, data):
        with open(self.init_data_path, mode='w') as file:
            for d in data:
                file.write(d+"\n")

    def clean_files(self):
        super(Connection, self).clean_files()

        try:
            os.remove(self.init_data_path)
        except:
            pass


def run():
    tests = [
        new_connection_test,
        known_connection_test,
        new_connection_to_known_test,
    ]

    for i in tests:
        print_result("connection: " + i.__name__, i())



"""
We give a new connection
"""
def new_connection_test():
    ret = True

    # CONFIG
    connection_filter = Connection()
    connection_filter.write_init_data(["42.42.42.1;42.42.42.2;1",
                                       "42.42.42.1;42.42.42.2;43;17"])
    connection_filter.write_conf()

    # START FILTER
    connection_filter.start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=connection_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            ["42.42.42.2","42.42.42.3","42","6"],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [100]
    expected_certitudes_size = 1

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")


    if len(certitudes) != 1:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    connection_filter.clean_files()
    connection_filter.stop()

    return ret

"""
We give a connection that have already 
been given in the init data file
"""
def known_connection_test():
    ret = True

    # CONFIG
    connection_filter = Connection()
    connection_filter.write_init_data(["42.42.42.1;42.42.42.2;1",
                                       "42.42.42.1;42.42.42.2;42;6"])
    connection_filter.write_conf()

    # START FILTER
    connection_filter.start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=connection_filter.socket,
                           socket_type="unix", )

    results = darwin_api.bulk_call(
        [
            ["42.42.42.1","42.42.42.2","1"],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0]
    expected_certitudes_size = 1

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    connection_filter.clean_files()
    connection_filter.stop()

    return ret

"""
We give a new connection, then we give it again
"""
def new_connection_to_known_test():
    ret = True

    # CONFIG
    connection_filter = Connection()
    connection_filter.write_init_data(["42.42.42.1;42.42.42.2;1",
                                       "42.42.42.1;42.42.42.2;42;6"])
    connection_filter.write_conf()

    # START FILTER
    connection_filter.start()

    # SEND TEST
    darwin_api = DarwinApi(socket_path=connection_filter.socket,
                           socket_type="unix", )

    darwin_api.bulk_call(
        [
            ["42.42.42.10","42.42.42.12","201", "6"],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    results = darwin_api.bulk_call(
        [
            ["42.42.42.10","42.42.42.12","201", "6"],
        ],
        filter_code="CONNECTION",
        response_type="back",
    )

    # VERIFY RESULTS
    certitudes = results.get('certitude_list')
    expected_certitudes = [0]
    expected_certitudes_size = 1

    if certitudes is None:
        ret = False
        logging.error("Connection Test : No certitude list found in result")


    if len(certitudes) != expected_certitudes_size:
        ret = False
        logging.error("Connection Test : Unexpected certitude size of {} instead of {}"
                      .format(len(certitudes), expected_certitudes_size))

    if certitudes != expected_certitudes:
        ret = False
        logging.error("Connection Test : Unexpected certitude of {} instead of {}"
                      .format(certitudes, expected_certitudes))

    # CLEAN
    darwin_api.close()

    connection_filter.clean_files()
    connection_filter.stop()

    return ret
