import os.path
import subprocess
import argparse
import json
from itertools import groupby
from pathlib import Path

handsan_path = "HandSanitizer"

parser = argparse.ArgumentParser()
parser.add_argument('input', help='foo help')

parser.add_argument('-g', '--generate', help='foo help', action="store_true")
parser.add_argument('--no-template', help='foo help', action="store_true")
parser.add_argument('functions', nargs='*')
# json unload the generated file
# sorted by purity setting

args = parser.parse_args()


def get_file_from_output_dir(input_file, ext):
    input_path = Path(input_file)
    stem = input_path.stem
    return Path(output_dir).joinpath(stem).with_suffix(ext)


output_dir = "output"
bc_file = args.input
generate_execs = args.generate
generate_input_template = args.no_template != True

template = ""
if not generate_input_template:
    template = "--no-template"


print(subprocess.run(["./HandSanitizer", bc_file, "-g", template, "-o", output_dir]))


specFile = get_file_from_output_dir(bc_file, '.spec')
with open(specFile, 'r') as j:
    contents = json.loads(j.read())
    funcs = contents['functions']

    print("-------- Functions --------")
    print("Pure:")
    for func in filter(lambda f: f['purity'] != "impure", funcs):
        print(func['name'], ",", func['purity'], ":", func['signature'])

    print("Impure:")
    for func in filter(lambda f: f['purity'] == "impure", funcs):
        print("\t", func['name'], ":", func['signature'])

if len(args.functions) > 0 and generate_execs:
    print("")
    for func_name in args.functions:

        subprocess.run(["./HandSanitizer", bc_file, template, "-o", output_dir, func_name])

        outputObj = get_file_from_output_dir(bc_file, ".o")
        subprocess.run(["llc", "-filetype=obj", bc_file, "-o",  outputObj])

        funcOutput = get_file_from_output_dir(func_name, "")

        funcP = get_file_from_output_dir(func_name, ".cpp")

        subprocess.run(["clang++", "-std=c++17", "skelmain.cpp", "-I.", outputObj, funcP, "-o", funcOutput])

