def print_results(results):

    for i in results:
        if i[1] is True:
            print("OK | " + i[0])
        else:
            print("ERROR | " + i[0])

def print_result(name, function):
    print(name + "... ", end='', flush=True)
    result = function()
    if result is True:
        print("\t\33[32m OK \33[0m")
    else:
        print("\t\033[91m ERROR \33[0m")
