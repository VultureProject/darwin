import json
import os
import datetime
from enum import IntEnum
from random import randint
from typing import List, Union
from dataclasses import dataclass

# This import will be resolved in the darwin python runtime
try:
    import darwin_logger
except ImportError:
    class darwin_logger:
        @staticmethod
        def log(level: int, msg: str):
            import sys
            outpipe = sys.stderr if level >= 4 else sys.stdout
            print(DarwinLogLevel(level).name, msg,file=outpipe)


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

class Execution:

    @staticmethod
    def filter_config(config: dict) -> bool:
        global threshold
        if 'dummy' not in config:
            darwin_log(DarwinLogLevel.Critical, 'The field \\"dummy\\" is not in the config')
            return False
        if 'threshold' in config:
            threshold = int(config['threshold'])
        if config.get('fail_conf', '') == 'yes':
            raise Exception('FAILED PYTHON SCRIPT CONFIGURATION')
        venv = config.get('python_venv_folder', '')
        if len(venv) != 0:
            import setuptools
            if venv in str(setuptools):
                darwin_log(DarwinLogLevel.Debug, 'WE ARE IN VIRTUAL ENV')
        return True

    def parse_body(self, body: str) -> Union[list, CustomData]:
        parsed = json.loads(body)
        if not isinstance(parsed, list):
            darwin_log(DarwinLogLevel.Error, 'input body is not a list')
            raise PythonFilterError("Parse Body: Wrong type")
        return parsed

    def filter_pre_process(self, parsed_data: Union[list, CustomData]) -> Union[list, CustomData]:
        for d in parsed_data:
            d = d.lower()
        return parsed_data

    def filter_process(self, pre_processed_data: Union[list, CustomData]) -> Union[list, CustomData, PythonFilterResponse]:
        resp = PythonFilterResponse('', [], [])
        for line in pre_processed_data:
            if isinstance(line, str):
                s = len(line)
                if s < 80:
                    resp.certitudes.append(s)
                    resp.body += line + os.linesep
                    write_context(line)
                else:
                    darwin_log(DarwinLogLevel.Warning, 'Line too long, skipping it, returning 101')
                    resp.certitudes.append(101)
                if 'alert:' in line:
                    alert = line.replace('alert:', '')
                    resp.alerts.append(alert)

        return resp

    def filter_contextualize(self, processed_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
        new_body = ''
        for line in processed_data.body.splitlines():
            new_body += str(query_context(line)) + ':' + line + os.linesep
        processed_data.body = new_body
        return processed_data

    def alert_contextualize(self, contextualized_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
        for alert in contextualized_data.alerts:
            if query_context('alert:' + alert) < threshold:
                darwin_log(DarwinLogLevel.Info, 'alert below threshold, skipping it')
                contextualized_data.alerts.remove(alert)
        return contextualized_data

    def alert_formating(self, contextualized_data: PythonFilterResponse) -> List[str]:
        formated_alerts = []
        for alert in contextualized_data.alerts:
            date = datetime.datetime.now()
            formated_alerts.append('{{"date":"{date}","alert":"{alert}"}}'.format(date=str(date), alert=alert))
        return formated_alerts


    def response_formating(self, contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
        return contextualized_data


    # WIP unused for now
    def output_formating(self, contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
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
