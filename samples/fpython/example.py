import json
from typing import List


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

def parse_body(input: str) -> list:
    parsed = json.loads(input)
    if not isinstance(parsed, list):
        raise PythonFilterError("Wrong type bro")
    return parsed

def filter_pre_process(input) -> list:
    return input

def filter_process(input: list) -> PythonFilterResponse:
    resp = PythonFilterResponse()
    for line in input:
        print(line)
        if isinstance(line, str):
            resp.certitudes.append(101)
            resp.alerts.append(line)
            resp.response += line
    return resp

def alert_formating(input: PythonFilterResponse) -> str:
    return json.dumps(input.alerts)

def output_formating(input: PythonFilterResponse) -> str:
    return json.dumps(input.response)

def response_formating(input: PythonFilterResponse) -> str:
    return json.dumps(input.response)
