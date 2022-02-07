import json
import os
import datetime
from enum import IntEnum
from typing import List, Union
from dataclasses import dataclass

# DOES NOT WORK , Used in tests to check if missing methods are detected by the filter

# This import will be resolved in the darwin python runtime
try:
    import darwin_logger
except ImportError:
    class darwin_logger:
        @staticmethod
        def log(level, msg):
            print(level, msg)

class DarwinLogLevel(IntEnum):
    Debug=0
    Info=1
    Notice=2
    Warning=3
    Error=4
    Critical=5

def darwin_log(level: DarwinLogLevel, msg:str):
    darwin_logger.log(int(level), msg)


class PythonFilterError(Exception):
    pass

@dataclass
class PythonFilterResponse:
    """
    Users may use this class or extend it or even use an entirely different class that has the same properties
    """
    body: str
    certitudes: List[int]
    alerts: List[str]

@dataclass
class CustomData:
    """
    Placeholder class, the output of parse_body is passed to filter_pre_process and the
    output of filter_pre_process is passed to filter_process
    """
    pass

threshold = 3

def filter_config(config: dict) -> bool:
    global threshold
    if 'dummy' not in config:
        darwin_log(DarwinLogLevel.Critical, 'The field \\"dummy\\" is not in the config')
        return False
    if 'threshold' in config:
        threshold = int(config['threshold'])
    return True

def parse_body(body: str) -> Union[list, CustomData]:
    parsed = json.loads(body)
    if not isinstance(parsed, list):
        darwin_log(DarwinLogLevel.Error, 'input body is not a list')
        raise PythonFilterError("Parse Body: Wrong type")
    return parsed

def filter_pre_process(parsed_data: Union[list, CustomData]) -> Union[list, CustomData]:
    for d in parsed_data:
        d = d.lower()
    return parsed_data

# MISSING METHOD filter_process

def filter_contextualize(processed_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
    new_body = ''
    for line in processed_data.body.splitlines():
        new_body += str(query_context(line)) + ':' + line + os.linesep
    processed_data.body = new_body
    return processed_data

def alert_contextualize(contextualized_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
    for alert in contextualized_data.alerts:
        if query_context('alert:' + alert) < threshold:
            darwin_log(DarwinLogLevel.Info, 'alert below threshold, skipping it')
            contextualized_data.alerts.remove(alert)
    return contextualized_data

def alert_formating(contextualized_data: PythonFilterResponse) -> List[str]:
    formated_alerts = []
    for alert in contextualized_data.alerts:
        date = datetime.datetime.now()
        formated_alerts.append('{{"date":"{date}","alert":"{alert}"}}'.format(date=str(date), alert=alert))
    return formated_alerts


def response_formating(contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
    return contextualized_data


# WIP unused for now
def output_formating(contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
    return contextualized_data


context = {}

def write_context(key:str, value=None):
    """Writes an information about the current task performed"""
    if key in context:
        context[key] += 1
    else:
        context[key] = 1

def query_context(key: str) -> int:
    """Query information about the current task performed"""
    if key in context:
        return context[key]
    else:
        return 0
