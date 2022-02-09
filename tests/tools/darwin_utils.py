import subprocess
import os
from time import sleep
from conf import DEFAULT_MANAGER_PATH, DEFAULT_PYTHON_EXEC, TEST_FILES_DIR


def darwin_start(darwin_manager_path=DEFAULT_MANAGER_PATH, config_path="{}/darwin.conf".format(TEST_FILES_DIR)):
    process = subprocess.Popen([
        DEFAULT_PYTHON_EXEC,
        darwin_manager_path,
        '-l',
        'DEBUG',
        '-p',
        TEST_FILES_DIR,
        '--no-suffix-directories',
        config_path
    ])

    sleep(6)
    return process

def darwin_stop(process):
    process.terminate()
    process.wait()

def darwin_configure(conf, path="{}/darwin.conf".format(TEST_FILES_DIR)):
    with open(path, mode='w') as file:
        file.write(conf)

def darwin_remove_configuration(path="{}/darwin.conf".format(TEST_FILES_DIR)):
    os.remove(path)

def count_file_lines(filepath):
    with open(filepath, 'r') as file:
        return len(file.readlines())