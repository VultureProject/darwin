from abc import abstractmethod
from typing import List, Union
from dataclasses import dataclass
from enum import IntEnum

try:
    import darwin_logger
except ImportError:
    class darwin_logger:
        @staticmethod
        def log(level: int, msg: str):
            import sys
            outpipe = sys.stderr if level >= 4 else sys.stdout
            print(DarwinLogLevel(level).name + ' : ' + msg,file=outpipe)


class DarwinLogLevel(IntEnum):
    Debug=0
    Info=1
    Notice=2
    Warning=3
    Error=4
    Critical=5

def darwin_log(level: DarwinLogLevel, msg:str):
    """
    Function to call to push logs to Darwin
    the module 'darwin_logger' is automatically loaded by Darwin
    """
    darwin_logger.log(int(level), msg)

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

#####################
##                 ##
##       API       ##
##                 ##
#####################
class Execution:

    @staticmethod
    @abstractmethod
    def filter_config(config: dict) -> bool:
        """
        Function called with the filter configuration
        This function is called only once when starting the filter
        """
        pass

    @abstractmethod
    def parse_body(self, body: str) -> Union[list, CustomData]:
        """
        Function called when receiving a new packet
        This implementation is optional, the default implementation parses a list of objects
        """
        pass

    @abstractmethod
    def filter_pre_process(self, parsed_data: Union[list, CustomData]) -> Union[list, CustomData]:
        """
        Function called after parse_body with the output of parse_body
        """
        pass

    @abstractmethod
    def filter_process(self, pre_processed_data: Union[list, CustomData]) -> Union[list, CustomData, PythonFilterResponse]:
        """
        Function called after filter_pre_process with the output of filter_pre_process
        This output will be forwarded to alert_formating, output_formating and response_formating
        """
        pass

    @abstractmethod
    def filter_contextualize(self, processed_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
        """
        Function called after filter_process with the output of filter_process
        It should usually call query_context to gather information about the processed_date in order to contextualize it
        The output of this function can extend PythonFilterResponse or may be any structure that has the
        attributes 'body: str', 'certitudes: List[int]' and 'alerts: List[str]'
        The output of the functions is forwarded to alert_contextualize, output_formating and response_formating
        """
        pass

    @abstractmethod
    def alert_contextualize(self, contextualized_data: Union[list, CustomData, PythonFilterResponse]) -> Union[CustomData, PythonFilterResponse]:
        """
        Function called between filter_contextualize and alert_formating
        It should be used to contextualize the alerts that should be risen by darwin
        The output of this function can extend PythonFilterResponse or may be any structure that has the
        attributes 'body: str', 'certitudes: List[int]' and 'alerts: List[str]'
        """
        pass


    @abstractmethod
    def alert_formating(self, contextualized_data: PythonFilterResponse) -> List[str]:
        """
        Function called after filter_process, each 'str' in the output will raise an alert
        """
        pass

    @abstractmethod
    # WIP This function is not wired, use only response_formating for now
    def output_formating(self, contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
        """
        Function called after filter_process, the output will be sent to the next filter as the body
        with the certitudes returned by filter_process
        """
        pass

    @abstractmethod
    def response_formating(self, contextualized_data: Union[CustomData, PythonFilterResponse]) -> PythonFilterResponse:
        """
        Function called after filter_process, the output will be sent to the sender as the body with
        the certitudes returned by filter_process
        """
        pass


# Context API

def write_context(key, value):
    """Writes an information about the current task performed"""
    pass

def query_context(key):
    """Query information about the current task performed"""
    pass