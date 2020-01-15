import os

def main(csv_input, csv_output, json_output):
    in_file = open(csv_input, 'r')
    out_file = open(csv_output, 'w')
    json_file = open(json_output, 'w')
    ret = True
    write = True
    clear_out_file = False

    for line in in_file:
        if "trigger_false" in line:
            ret = False
        if "trigger_no_write" in line:
            write = False
        if "trigger_no_out_file" in line:
            clear_out_file = True
        if write:
            out_file.write(line)

    in_file.close()
    out_file.close()
    json_file.close()

    if clear_out_file:
        os.remove(csv_output)
        os.remove(json_output)

    return ret
