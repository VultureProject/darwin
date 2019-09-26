def print_results(results):

    for i in results:
        if i[1] is True:
            print("OK | " + i[0])
        else:
            print("ERROR | " + i[0])

def print_result(name, result):
    if result is True:
        print("OK | " + name)
    else:
        print("ERROR | " + name)
