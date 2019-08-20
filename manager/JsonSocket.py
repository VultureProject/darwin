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
