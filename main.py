import os.path
import subprocess
import argparse
import json
from itertools import groupby
from pathlib import Path

handsan_path = "HandSanitizer"

parser = argparse.ArgumentParser()
parser.add_argument('input', help='input bitcode file')

parser.add_argument('-b', '--build', help='build executables for functions',
                    action="store_true")
parser.add_argument('--opt', help='runs opt -O1 first on the bc file, this does purity analysis for us',
                    action="store_true")
parser.add_argument('--no-template', help='prevents the generation of json input templates', action="store_true")
parser.add_argument('functions', nargs='*')

args = parser.parse_args()


def get_filepath_in_output_dir(input_file, ext):
    input_path = Path(input_file)
    stem = input_path.stem
    return Path(output_dir).joinpath(stem).with_suffix(ext)


def gen_spec_and_list_functions(bc_file: str, template: str, output_dir:str):
    subprocess.run(["./HandSanitizer", bc_file, "-g", template, "-o", output_dir])
    specFile = get_filepath_in_output_dir(bc_file, '.spec')
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


def build_functions_for(bc_file: str, function_names: [str], template: str, output_dir: str):
    for func_name in function_names:
        subprocess.run(["./HandSanitizer", bc_file, template, "-o", output_dir, func_name])
        outputObj = get_filepath_in_output_dir(bc_file, ".o")
        subprocess.run(["llc", "-filetype=obj", bc_file, "-o", outputObj])

        funcOutput = get_filepath_in_output_dir(func_name, "")
        funcP = get_filepath_in_output_dir(func_name, ".cpp")

        subprocess.run(["clang++", "-std=c++17", "skelmain.cpp", "-I.", outputObj, funcP, "-o", funcOutput])


output_dir = "output"
bc_file = args.input
build_execs = args.build
generate_input_template = args.no_template != True
template = ""
if not generate_input_template:
    template = "--no-template"

if build_execs:
    build_functions_for(bc_file, args.functions, template, output_dir)
else:
    gen_spec_and_list_functions(bc_file, template, output_dir)