__author__ = "Hugo SOSZYNSKI"
__credits__ = []
__license__ = "GPLv3"
__version__ = "3.0.0"
__maintainer__ = "Vulture Project"
__email__ = "contact@vultureproject.org"
__doc__ = 'JSON encoding and decoding socket encapsulation class'

import json


class JsonSocket:
    """
    Encapsulation of a socket made to send and receive JSON.
    """

    def __init__(self, sock):
        """
        Constructor. Set timeout to the socket.

        :param sock: The socket to encapsulate.
        """
        self._socket = sock
        self._socket.settimeout(1)

    def __del__(self):
        """
        Destructor. Close the socket.
        """
        self._socket.close()

    def send(self, data):
        """
        Send the given data JSON formatted through the socket.

        :param data: The data to send.
        """
        j = json.dumps(data)
        self._socket.send(j.encode('ascii'))

    def recv(self):
        """
        Receive JSON formatted data from the socket.

        :return: Dict object containing the deserialized JSON data.
        """
        data = ''
        cpt = 0
        first_chunk = True
        while True:
            chunk = self._socket.recv(2048)
            chunk = chunk.decode().rstrip('\x00')
            if not chunk:
                raise ValueError('No data received')
            for c in chunk:
                if first_chunk:
                    if c != '{':
                        raise TypeError('Data received not a json')
                    cpt += 1
                    first_chunk = False
                else:
                    if c == '{':
                        cpt += 1
                    elif c == '}':
                        cpt -= 1
            data += chunk

            # Full JSON received ?
            if cpt == 0:
                return json.loads(data)
