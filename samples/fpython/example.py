import json
from typing import Any, List


class PythonFilterError(Exception):
    pass

class PythonFilterResponse:
    body: str
    certitudes: List[int]
    alerts: List[str]
    def __init__(self) -> None:
        self.body = ''
        self.certitudes = []
        self.alerts = []
        self.response = ''

def filter_config(config: Any) -> bool:
    print('filter', config)
    return True

def parse_body(input: str) -> list:
    parsed = json.loads(input)
    if not isinstance(parsed, list):
        raise PythonFilterError("Wrong type bro")
    return parsed

def filter_pre_process(input) -> list:
    return input

def filter_process(input: list) -> PythonFilterResponse:
    print('py hi')
    print(input)
    resp = PythonFilterResponse()
    for line in input:
        print('py line')
        # print(line)
        if isinstance(line, str):
            resp.certitudes.append(101)
            resp.alerts.append(line)
            resp.response += line

    print('py ret')
    return resp

def alert_formating(input: PythonFilterResponse) -> str:
    print('alert')
    return input.alerts

def output_formating(input: PythonFilterResponse) -> str:
    print('output')
    return input

def response_formating(input: PythonFilterResponse) -> str:
    print('output')
    return input
