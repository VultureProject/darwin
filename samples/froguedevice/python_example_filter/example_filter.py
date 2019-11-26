# we need to import a package here, to test our Python environment
from myPackage.somePython import fahrToKelv

# we could imagine a complicated machine learning function which in the end returns a result
def my_super_machine_learning_function(fahrenheit_temperature=32):
    return fahrToKelv(fahrenheit_temperature)
