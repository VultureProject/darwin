import logging
import json
import tempfile
import venv
from tools.filter import Filter
from tools.output import print_result

class PythonFilter(Filter):
    alert_file='/tmp/python_alerts.log'
    threshold=3
    def __init__(self):
        super().__init__(filter_name="python")

    def configure(self, content=None, venv='', other_conf=''):
        if not content:
            content = '{{\n' \
                        '"python_script_path": "{python_path}",\n' \
                        '"shared_library_path": "{so_path}",\n' \
                        '"log_file_path": "{alert_file}",\n' \
                        '"python_venv_folder": "{venv}",\n' \
                        '"dummy":"",\n' \
                        '{other_conf}' \
                        '"threshold":{threshold}\n' \
                    '}}'.format(python_path='tests/filters/data/example.py',
                                alert_file=self.alert_file,
                                threshold=self.threshold,
                                venv=venv,
                                other_conf=other_conf,
                                so_path='')
        super(PythonFilter, self).configure(content)




def run():
    tests = [
        test_bad_config_no_script,
        test_bad_config_empty_python_script_path,
        test_bad_config_nothing,
        test_bad_config_no_venv,
        test_wrong_config_missing_method,
        test_wrong_python_missing_requirement,
        test_wrong_python_exception_during_conf,
        test_wrong_python_exception_during_init,
        test_wrong_python_exception_during_steps,
        test_wrong_python_exception_during_delete,        
        test_venv,
        full_python_functional_test,
    ]

    for i in tests:
        print_result("Python: " + i.__name__, i)

def test_bad_config_no_script():
    f = PythonFilter()

    f.configure('{"python_script_path":"/tmp/no_script_there.bad_ext", "shared_library_path":""}')
    if f.valgrind_start():
        logging.error('test_bad_config_no_script: Filter should not start')
        return False
    if not f.check_line_in_filter_log('Generator::LoadPythonScript : Error loading the python script : failed to open'):
        logging.error('test_bad_config_no_script: Filter should have failed with a log')
        return False
    return True

def test_bad_config_empty_python_script_path():
    f = PythonFilter()

    f.configure('{"shared_library_path":"/tmp/no_script_there.bad_ext", "python_script_path":""}')
    if f.valgrind_start():
        logging.error('test_bad_config_no_shared_obj: Filter should not start')
        return False
    if not f.check_line_in_filter_log('Generator::LoadPythonScript : No python script to load'):
        logging.error('test_bad_config_no_shared_obj: Filter should have failed with a log')
        return False
    return True

def test_bad_config_nothing():
    f = PythonFilter()

    f.configure('{"shared_library_path":"", "python_script_path":""}')
    if f.valgrind_start():
        logging.error('test_bad_config_nothing: Filter should not start')
        return False
    if not f.check_line_in_filter_log('Generator::LoadPythonScript : No python script to load'):
        logging.error('test_bad_config_nothing: Filter should have failed with a log')
        return False
    return True

def test_bad_config_no_venv():
    f = PythonFilter()
        
    f.configure(venv='/tmp/VenvPythonFilterDoesNotExist')
    if f.valgrind_start():
        logging.error('test_bad_config_no_venv: Filter should not start')
        return False
    
    if not f.check_line_in_filter_log('Generator::LoadPythonScript : Virtual Env does not exist : '):
        logging.error('test_bad_config_no_venv: Filter should have failed with a log')
        return False
    return True

def test_wrong_config_missing_method():
    f = PythonFilter()
        
    f.configure('{"python_script_path":"tests/filters/data/bad_example.py", "shared_library_path":""}')
    if f.valgrind_start():
        logging.error('test_wrong_config_missing_method: Filter should not start')
        return False
    
    if not f.check_line_in_filter_log('Generator::CheckConfig : Mandatory methods were not found in the python script or the shared library'):
        logging.error('test_wrong_config_missing_method: Filter should have failed with a log')
        return False
    return True

def test_wrong_python_missing_requirement():
    f = PythonFilter()
        
    f.configure('{"python_script_path":"tests/filters/data/bad_example_bad_import.py", "shared_library_path":""}')
    if f.valgrind_start():
        logging.error('test_wrong_python_missing_requirement: Filter should not start')
        return False
    
    if not f.check_line_in_filter_log('Generator::'):
        logging.error('test_wrong_python_missing_requirement: Filter should have failed with a log')
        return False
    return True

def test_wrong_python_exception_during_conf():
    f = PythonFilter()
        
    f.configure(other_conf='"fail_conf":"yes",\n')
    if f.valgrind_start():
        logging.error('test_wrong_python_exception_during_conf: Filter should not start')
        return False
    
    if not f.check_line_in_filter_log('FAILED PYTHON SCRIPT CONFIGURATION'):
        logging.error('test_wrong_python_exception_during_conf: Filter should have failed with a log')
        return False
    return True

def test_wrong_python_exception_during_steps():
    f = PythonFilter()
        
    f.configure()
    if not f.valgrind_start():
        logging.error('test_wrong_python_exception_during_steps: Filter should start')
        return False
    # Error: line too long
    res = f.send_single("fail"*25)
    if res != 101:
        logging.error('test_wrong_python_exception_during_steps: Filter should fail with an error cerittude (101), but we received: ' + str(res))
        return False

    if not f.check_run():
        logging.error('test_wrong_python_exception_during_steps: Filter should still be running')
        return False

    return True

def test_wrong_python_exception_during_init():
    f = PythonFilter()
        
    f.configure('{"python_script_path":"tests/filters/data/bad_example_fail_init.py", "shared_library_path":"", "dummy":""}')
    if not f.valgrind_start():
        logging.error('test_wrong_python_exception_during_init: Filter should start')
        return False
    # Error: exception raised on third init
    is_test_ok = True
    for i in range(5):
        line = 'hello'
        res = f.send_single(line)
        if i == 3 and res != None:
            is_test_ok = False
            logging.error('test_wrong_python_exception_during_init: Filter should fail with an error cerittude (101), but we received: ' + str(res))
        if i == 3 and not f.check_line_in_filter_log("PythonTask:: PythonTask:: Error creating Execution object: Python error '<class 'Exception'>' : Fail at init"):
            is_test_ok = False
            logging.error('test_wrong_python_exception_during_init: Filter should fail with a specific log, but was not found')
        if i != 3 and res != len(line):
            is_test_ok = False
            logging.error(f'test_wrong_python_exception_during_init: Filter should succced with a certitude of ({str(len(line))}), but we received: {str(res)}')

    if not f.check_run():
        logging.error('test_wrong_python_exception_during_init: Filter should still be running')
        return False

    return is_test_ok

def test_wrong_python_exception_during_delete():
    """
    Cannot be tested as exceptions during __del__ are ignored by the python interpreter
    """
    return True


def test_venv():
    with tempfile.TemporaryDirectory() as tmpdir:
        e = venv.EnvBuilder(with_pip=True)
        e.create(tmpdir)
        
        f = PythonFilter()
        f.configure(venv=tmpdir)
        if not f.valgrind_start():
            logging.error('test_venv: Filter should start')
            return False
        
        if not f.check_line_in_filter_log('WE ARE IN VIRTUAL ENV'):
            logging.error('test_venv: Filter should have logged')
            return False

    return True

# Functional test with exemple.py, it should go through all python steps
def full_python_functional_test():
    f = PythonFilter()
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
