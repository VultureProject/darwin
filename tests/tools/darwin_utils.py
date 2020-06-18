import subprocess
import os
from time import sleep
from conf import DEFAULT_MANAGER_PATH, DEFAULT_CONFIGURATION_PATH, DEFAULT_PYTHON_EXEC


def darwin_start(darwin_manager_path=DEFAULT_MANAGER_PATH, config_path=DEFAULT_CONFIGURATION_PATH):
    process = subprocess.Popen([
        DEFAULT_PYTHON_EXEC,
        darwin_manager_path,
        '-l',
        'DEBUG',
        '-p',
        'tmp',
        '--no-suffix-directories',
        config_path
    ])

    sleep(3)
    return process

def darwin_stop(process):
    process.terminate()
    process.wait()

def darwin_configure(conf, path=DEFAULT_CONFIGURATION_PATH):
    with open(path, mode='w') as file:
        file.write(conf)

def darwin_remove_configuration(path=DEFAULT_CONFIGURATION_PATH):
    os.remove(path)