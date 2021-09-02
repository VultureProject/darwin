from tools.filter import Filter
from tools.output import print_result

class PythonFilter(Filter):
    def __init__(self):
        super().__init__(filter_name="python")

    def configure(self, content=None):
        if not content:
            content = '{{\n' \
                        '"python_script_path": "{python_path}",\n' \
                        '"shared_library_path": "{so_path}",\n' \
                        '"log_file_path": "/tmp/python_alerts.log"\n' \
                    '}}'.format(python_path='/home/myadvens.lan/tcartegnie/workspace/darwin/samples/fpython/example.py',
                                so_path='/home/myadvens.lan/tcartegnie/workspace/pycpp/build/libfpython.so')
        print(content)
        super(PythonFilter, self).configure(content)




def run():
    tests = [
        test
    ]

    for i in tests:
        print_result("Python: " + i.__name__, i)

def test():
    f = PythonFilter()
    print(' '.join(f.cmd))
    f.configure()
    f.valgrind_start()
    print('ret:' , f.send_single("hello"))
    return True