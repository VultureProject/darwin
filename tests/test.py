import logging
from sys import stderr
import manager_socket.test as manager_socket


if __name__ == "__main__":
    logging.basicConfig(filename="test_error.log", filemode='w', level=logging.ERROR)

    manager_socket.run()

    print("Note: you can read test_error.log for more details", file=stderr)
