import sys
import os
import socket
import subprocess
from time import sleep
from conf import MANAGEMENT_SOCKET_PATH


def requests(request):

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)

    try:
        sock.connect(MANAGEMENT_SOCKET_PATH)
    except socket.error as msg:
        print(msg, file=sys.stderr)
        return None
    try:
        sock.sendall(bytes(request))
    except Exception as e:
        print("Error: Could not send the request: " + str(e), file=sys.stderr)
        return None

    response = sock.recv(4096).decode()

    return response
