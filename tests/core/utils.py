from conf import DEFAULT_FILTER_PATH
from tools.filter import DEFAULT_ALERTS_FILE


DEFAULT_PATH = DEFAULT_FILTER_PATH + "darwin_logs"
RESP_MON_STATUS_RUNNING = '"status": "running"'
FTEST_CONFIG = f'{{"log_file_path": "{DEFAULT_ALERTS_FILE}"}}'
FTEST_CONFIG_NO_ALERT_LOG = '{}'