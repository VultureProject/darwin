import subprocess
import logging
import os
import redis
from time import sleep

class RedisServer():
    def __init__(self, address=None, unix_socket=None, port=7890, master=None):
        self.address = address
        self.unix_socket = unix_socket
        self.port = port
        self.process = None
        self.config_file = "/tmp/redis_conf_{port}.conf".format(port=self.port)
        self.master = master
        self.cmd = ["redis-server", self.config_file]

        if not self.address and not self.unix_socket:
            raise Exception("Redis needs to have at least an address or a unix socket to connect to")

        self.write_conf()
        self.start()

    def write_conf(self):
        with open(self.config_file, "w") as f:
            if self.address:
                f.write("bind " + self.address + "\r\n")
                f.write("port " + str(self.port) + "\r\n")
            if self.unix_socket:
                f.write("unixsocket " + self.unix_socket + "\r\n")

            if self.master:
                if self.master.address:
                    f.write("slaveof " + self.master.address + " " + str(self.master.port) + "\r\n")

    def clear_conf(self):
        try:
            os.remove(self.config_file)
        except:
            pass

    def start(self):
        self.process = subprocess.Popen(self.cmd, stdout=subprocess.PIPE)
        sleep(1)

    def stop(self):
        self.process.terminate()
        try:
            self.process.wait(3)
        except subprocess.TimeoutExpired:
            logging.error("Unable to stop Redis server correctly. Killing it.")
            self.process.kill().wait()
            return False
        return True

    def __del__(self):
        if self.process and self.process.poll() is None:
            self.stop()
        self.clear_conf()

    def connect(self):
        if self.address:
            r = redis.Redis(host=self.address, port=self.port, db=0)
        else:
            r = redis.Redis(unix_socket_path=self.unix_socket, db=0)

        return r

    def get_number_of_connections(self):
        connection = self.connect()

        ret = connection.client_list()

        return len(ret)