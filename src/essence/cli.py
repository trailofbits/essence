import os.path
import pathlib
import subprocess
import argparse
import json
from itertools import groupby
from pathlib import Path


def hello():
    print("hello essence!")


def get_filepath_in_output_dir(output_dir: str, input_file: str, ext: str):
    input_path = Path(input_file)
    stem = input_path.stem
    return Path(output_dir).joinpath(stem).with_suffix(ext)


handsan_path = "HandSanitizer"


# main entry point
def essence():
    print(os.path.dirname(os.path.realpath(__file__)))
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='input bitcode file')
    parser.add_argument('-o', '--output',
                        help='folder in which executables will be saved / or output path for spec file',
                        nargs='?', default='output')
    parser.add_argument('-b', '--build', help='build executables for functions',
                        action="store_true")
    parser.add_argument('-g', '--generate-spec', help='generates specification of bitcode module', action="store_true")
    parser.add_argument('--no-template', help='prevents the generation of json input templates', action="store_true")
    parser.add_argument('functions', nargs='*',
                        help='functions from the specified bitcode module which need to be build')
    args = parser.parse_args()

    input_file = args.input
    output_dir = args.output
    build_execs = args.build
    generate_spec = args.generate_spec
    generate_input_template = args.no_template != True
    functions_to_build = args.functions

    if pathlib.Path(input_file).suffix == '.bc':
        if build_execs:
            essence_build(input_file, output_dir, generate_input_template, functions_to_build)

        elif generate_spec:
            essence_generate_spec(input_file, output_dir)

        else:
            essence_generate_spec(input_file, output_dir)
            path_to_spec_file = get_filepath_in_output_dir(output_dir, input_file, ".spec.json")
            essence_list_signatures(path_to_spec_file)
    else:
        essence_list_signatures(input_file)


#
#
#
# # lists function signatures
def essence_list_signatures(spec_file: str):
    with open(spec_file, 'r') as j:
        contents = json.loads(j.read())
        funcs = contents['functions']

        print("-------- Functions --------")
        print("Pure:")
        for func in filter(lambda f: f['purity'] != "impure", funcs):
            print(func['name'], ",", func['purity'], ":", func['signature'])

        print("Impure:")
        for func in filter(lambda f: f['purity'] == "impure", funcs):
            print("\t", func['name'], ":", func['signature'])


#
#
#
# # generates and prints spec
def essence_generate_spec(bc_file: str, output: str):
    subprocess.run(["./HandSanitizer", bc_file, "-o", output])


#
#
#
# # build the actual functions
def essence_build(bc_file: str, output: str, generate_input_template: bool, function_names: [str]):
    print("----------------- building functions --------------------")
    for func_name in function_names:
        print(f"building: {func_name}")
        build_functions_for(bc_file, output, generate_input_template, func_name)


def build_functions_for(bc_file: str, output_dir: str, template: bool, func_name: str):
    if template:
        subprocess.run(["./HandSanitizer", "--build", "-o", output_dir, bc_file, func_name])
    else:
        subprocess.run(["./HandSanitizer", "--build", "-o", output_dir, bc_file, func_name, "--no-template"])

    output_obj_file_path = get_filepath_in_output_dir(output_dir, bc_file, ".o")
    subprocess.run(["llc", "-filetype=obj", bc_file, "-o", output_obj_file_path])

    func_exec_file_path = get_filepath_in_output_dir(output_dir, func_name, "")
    func_generated_cpp_file_path = get_filepath_in_output_dir(output_dir, func_name, ".cpp")

    subprocess.run(
        ["clang++", "-std=c++17", "skelmain.cpp", "-I.", output_obj_file_path, func_generated_cpp_file_path, "-o",
         func_exec_file_path])

# TODO later
# def essence_build_from_spec(bc_file: str):
