from tools.filter import Filter
from tools.output import print_result
import pandas as pd
import json
import datetime
import logging

class PythonFilter(Filter):
    alert_file='/tmp/python_alerts.log'
    threshold=3
    def __init__(self):
        super().__init__(filter_name="python")

    def configure(self, content=None):
        if not content:
            content = '{{\n' \
                        '"python_script_path": "{python_path}",\n' \
                        '"shared_library_path": "{so_path}",\n' \
                        '"log_file_path": "{alert_file}",\n' \
                        '"python_venv_folder": "/home/myadvens.lan/tcartegnie/workspace/darwin/.venv/",' \
                        '"dummy":"",\n' \
                        '"threshold":{threshold}\n' \
                    '}}'.format(python_path='samples/fpython/example.py',
                                alert_file=self.alert_file,
                                threshold=self.threshold,
                                so_path='')
                                # so_path='/home/myadvens.lan/tcartegnie/workspace/pycpp/build/libfpython.so')
        print(content)
        super(PythonFilter, self).configure(content)




def run():
    tests = [
        full_python_functional_test,
        # test_ueba
    ]

    for i in tests:
        print_result("Python: " + i.__name__, i)

# Functional test with exemple.py, it should go through all python steps
def full_python_functional_test():
    f = PythonFilter()
    print(' '.join(f.cmd))
    f.threshold=5
    f.configure()
    f.valgrind_start()
    api = f.get_darwin_api(verbose=False)
    is_test_ok =True
    for i in range(100):
        line = 'hello ! ' + str(i)
        ret = api.call(line , response_type="back")
        if ret != len(line):
            logging.error('line {} returned a bad certitude : {} instead of {}'.format(str(i), ret, len(line)))
            is_test_ok = False

        line = 'alert:hello wow ! ' + str(i%10) 
        ret = api.call(line , response_type="back")
        if ret != len(line):
            logging.error('line {} returned a bad certitude : {} instead of {}'.format(str(i), ret, len(line)))
            is_test_ok = False
    # This next line should be too long, hence it should return a 101 certitude
    ret = api.call('hello ! '*20 , response_type="back")
    if ret != 101:
            logging.error('line {} returned a bad certitude : {} instead of {}'.format(str(i), ret, 101))
            is_test_ok = False
    if not is_test_ok:
        return False
    
    lines = []
    for i in range(100):
        lines.append('bulk hello ! ' + str(i))
        lines.append('alert:bulk hello wow ! ' + str(i%10))
    lines.append('hello ! '*20)

    # empty the log file
    with open(f.alert_file, 'w') as _:
        pass

    ret=api.bulk_call(lines,response_type='back')
    predicted_alerts={}
    for i, l in enumerate(lines):
        if l.startswith('alert:'):
            tmp = l.replace('alert:', '')
            if tmp in predicted_alerts:
                predicted_alerts[tmp] += 1
            else:
                predicted_alerts[tmp] = 1
    
    with open(f.alert_file, 'r') as file:
        alerts_risen = file.readlines()
    
    is_test_ok = True
    for alert in alerts_risen:
        try:
            json_alert = json.loads(alert)
        except json.JSONDecodeError as e:
            is_test_ok=False
            logging.error('Malformed alert : should be a valid json', exc_info=e)
            continue
        if "date" not in json_alert or "alert" not in json_alert:
            is_test_ok=False
            logging.error('Malformed alert : "date" or "alert" not present ' + alert)
            continue
        if json_alert['alert'] not in predicted_alerts:
            is_test_ok=False
            logging.error('Malformed alert : alert should not have been triggered ' + alert)
            continue
        if predicted_alerts[json_alert['alert']] < f.threshold:
            is_test_ok=False
            logging.error('Malformed alert : alert should not have been triggered (below threshold) ' + alert)
            continue
    
    api.close()
    f.valgrind_stop()

    if not is_test_ok:
        return False
    return True

def test_ueba():
    f = PythonFilter()
    f.configure('{{\n' \
                        '"python_script_path": "{python_path}",\n' \
                        '"shared_library_path": "{so_path}",\n' \
                        '"log_file_path": "/tmp/python_alerts.log",\n' \
                        '"model_path": "/tmp/aggro.pkl",\n' \
                        '"data_path": "/tmp/anomalies.csv"\n'
                    '}}'.format(python_path='/home/myadvens.lan/tcartegnie/workspace/ueba_poc-feature-ueba_bootstrap/aggro_ueba.py',
                                # so_path='/home/myadvens.lan/tcartegnie/workspace/ueba_aggro_rs/target/debug/libueba_aggro_rs.so'))
                                so_path=''))
    
    f.valgrind_start()
    # sleep(60)
    api = f.get_darwin_api(verbose=False)

    df_raw = pd.read_csv('/home/myadvens.lan/tcartegnie/workspace/ueba_poc-feature-ueba_bootstrap/samples/win_ad_hashed.csv')

    for day in pd.date_range(datetime.date.fromisoformat(df_raw.date.min()), datetime.date.fromisoformat(df_raw.date.max())):
        batch = df_raw[df_raw.date == str(day.strftime('%Y-%m-%d'))].copy()
        payload = [json.loads(batch.to_json())]
        
        start = datetime.datetime.now()
        ret = api.bulk_call(payload, response_type="back")
        end = datetime.datetime.now()

        print(day, ret, end-start, len(batch))
    return True