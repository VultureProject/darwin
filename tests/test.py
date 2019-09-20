import logging
from sys import stderr
import manager_socket.test as manager_socket
import core.test as core


if __name__ == "__main__":
    logging.basicConfig(filename="test_error.log", filemode='w', level=logging.ERROR)

    core.run()
    manager_socket.run()

    print("Note: you can read test_error.log for more details", file=stderr)
