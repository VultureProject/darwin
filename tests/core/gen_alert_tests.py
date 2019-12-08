
def build_conf(line: str) -> str:
    conf = '{'
    if 'empty_file' in line:
        conf += '"log_file_path": EMPTY_ALERT_FILE,'
    elif 'wrong_file' in line:
        conf += '"log_file_path": WRONG_ALERT_FILE,'
    elif 'file' in line:
        conf += '"log_file_path": ALERT_FILE,'

    if 'empty_socket' in line:
        conf += '"redis_socket_path": EMPTY_REDIS_SOCKET,'
    elif 'wrong_socket' in line:
        conf += '"redis_socket_path": WRONG_REDIS_SOCKET,'
    elif 'socket' in line:
        conf += '"redis_socket_path": REDIS_SOCKET,'

    if 'empty_list' in line:
        conf += '"alert_redis_list_name": EMPTY_REDIS_ALERT_LIST,'
    elif 'wrong_list' in line:
        conf += '"alert_redis_list_name": WRONG_REDIS_ALERT_LIST,'
    elif 'list' in line:
        conf += '"alert_redis_list_name": REDIS_ALERT_LIST,'

    if 'empty_channel' in line:
        conf += '"alert_redis_channel_name": EMPTY_REDIS_ALERT_CHANNEL,'
    elif 'wrong_channel' in line:
        conf += '"alert_redis_channel_name": WRONG_REDIS_ALERT_CHANNEL,'
    elif 'channel' in line:
        conf += '"alert_redis_channel_name": REDIS_ALERT_CHANNEL,'

    if conf[-1] == ',':
        conf = conf[:-1]
    conf += '}'
    return conf


def is_expected(line: str, media: str) -> str:
    if 'empty_' + media in line or 'wrong_' + media in line:
        return False
    elif media in line:
        return True
    return False


with open('tests.txt', 'r') as f:
    lines = f.readlines()

a = 4
for i in lines:
    conf = build_conf(i)
    test = '{{"test_name": "{}", "conf": {}, "log": "this is log {}", "expected_file": {}, "expected_list": {}, "expected_channel": {}}},'.format(
        i[:-1],
        conf,
        a,
        is_expected(i, 'file'),
        is_expected(i, 'list'),
        is_expected(i, 'channel')
    )
    a += 1
    print(test)
