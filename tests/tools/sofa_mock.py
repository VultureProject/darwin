def main(cvs_input, cvs_output, json_output):
    in_file = open(cvs_input, 'r')
    out_file = open(cvs_output, 'w')

    for line in in_file:
        out_file.write(line)

    in_file.close()
    out_file.close()

    return True
