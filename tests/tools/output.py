def print_results(results):

    for i in results:
        if i[1] is True:
            print("OK | " + i[0])
        else:
            print("ERROR | " + i[0])

def print_result(name, function):
    result = function()
    if result is True:
        print(name + ": " + "\33[32m OK \33[0m", flush=True)
    else:
        print(name + " :" + "\033[91m ERROR \33[0m", flush=True)
