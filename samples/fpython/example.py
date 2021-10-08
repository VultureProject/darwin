import json
from typing import List, Union
from dataclasses import dataclass

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

def filter_config(config: dict) -> bool:
    if 'dummy' not in config:
        return False
    return True

def parse_body(body: str) -> Union[list, CustomData]:
    parsed = json.loads(body)
    if not isinstance(parsed, list):
        raise PythonFilterError("Parse Body: Wrong type")
    return parsed

def filter_pre_process(parsed_data: Union[list, CustomData]) -> Union[list, CustomData]:
    return parsed_data

def filter_process(pre_processed_data: Union[list, CustomData]) -> Union[list, CustomData, PythonFilterResponse]:
    resp = PythonFilterResponse('', [], [])
    for line in pre_processed_data:
        if isinstance(line, str):
            resp.certitudes.append(101)
            resp.alerts.append(line)
            resp.body += line

    print('py ret')
    return resp

def filter_contextualize(processed_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
    return processed_data

def alert_contextualization(contextualized_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
    return contextualized_data

def alert_formating(contextualized_data: PythonFilterResponse) -> List[str]:
    print('alert')
    return contextualized_data.alerts

def output_formating(contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
    print('output')
    return contextualized_data

def response_formating(contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
    print('output')
    return contextualized_data
