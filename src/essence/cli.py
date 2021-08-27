import os.path
import pathlib
import subprocess
import argparse
import json
from itertools import groupby
from pathlib import Path


# TODO: Handle possible functions called main in essence not handsanitizer


def get_filepath_in_output_dir(output_dir: str, input_file: str, ext: str) -> Path:
    input_path = Path(input_file)
    stem = input_path.stem
    return Path(output_dir).joinpath(stem).with_suffix(ext)


dirname = os.path.dirname(__file__)
handsan_path = dirname + "/../../handsan"


# main entry point
def essence():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='input bitcode file')
    parser.add_argument('-o', '--output',
                        help='folder in which executables will be saved / or output path for spec file',
                        nargs='?', default='output')
    parser.add_argument('-b', '--build', help='build executables for functions',
                        action="store_true")
    parser.add_argument('-g', '--generate-spec', help='generates specification of bitcode module', action="store_true")
    parser.add_argument('--no-template', help='prevents the generation of json input templates', action="store_true")
    parser.add_argument('--build-read-none', help='build all functions that have purity level: read none', action="store_true")
    parser.add_argument('--build-write-only', help='build all functions that have purity level: write only', action="store_true")
    parser.add_argument('functions', nargs='*',
                        help='functions from the specified bitcode module which need to be build')
    args = parser.parse_args()

    input_file = Path(args.input)
    output_dir = args.output
    build_execs = args.build
    generate_spec = args.generate_spec
    generate_input_template = args.no_template != True
    functions_to_build = args.functions
    build_read_none = args.build_read_none
    build_write_only = args.build_write_only

    if input_file.suffix == '.c':
        print("got .c file as input, you probably meant .bc", file=sys.stderr)
        return;

    if input_file.suffix == '.bc':
        if build_execs:
            essence_build(input_file, output_dir, generate_input_template, functions_to_build)

        elif build_read_none or build_write_only:
            if build_read_none:
                essence_build_read_none(input_file, output_dir, generate_input_template)
            if build_write_only:
                essence_build_write_only(input_file, output_dir, generate_input_template)

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
def essence_list_signatures(spec_file: Path):
    with spec_file.open("r") as j:
        contents = json.load(j)
        funcs = contents['functions']

        funcs.sort(key=lambda content: content['purity'])
        groups = groupby(funcs, lambda content: content['purity'])

        print("-------- Functions --------")
        for purity, funcs in groups:
            print(purity + ":")
            for func in funcs:
                print("\t", func['name'] + ":", func['signature'])


#
# # generates and prints spec
def essence_generate_spec(bc_file: Path, output: str):
    subprocess.run([handsan_path, "-o", output, bc_file])


#
#
#
# # build the actual functions
def essence_build(bc_file: Path, output: str, generate_input_template: bool, function_names: [str]):
    print("----------------- building functions --------------------")
    for func_name in function_names:
        print(f"building: {func_name}")
        build_functions_for(bc_file, output, generate_input_template, func_name)



def essence_build_read_none(bc_file: Path, output: str, generate_input_template: bool):
    essence_build_for_purity_level(bc_file, output, generate_input_template, 'read_none')

def essence_build_write_only(bc_file: Path, output: str, generate_input_template: bool):
    essence_build_for_purity_level(bc_file, output, generate_input_template, 'write_only')


def essence_build_for_purity_level(bc_file: Path, output: str, generate_input_template: bool, purity_level :str):
    essence_generate_spec(bc_file, output)
    spec_path = get_filepath_in_output_dir(output, bc_file, '.spec.json')
    with spec_path.open("r") as j:
        contents = json.load(j)
        funcs = contents['functions']
        funcs = [func for func in funcs if func['purity'] == purity_level]
        print("----------------- building " + purity_level  + " functions --------------------")
        for func in funcs:
            print('building', func['name'])
            build_functions_for(bc_file, output, generate_input_template, func['name'])





def build_functions_for(bc_file: Path, output_dir: str, template: bool, func_name: str):
    extracted_bc_path = get_filepath_in_output_dir(output_dir, bc_file.stem, ".extracted.bc")
    subprocess.run(["llvm-extract", bc_file, "--recursive", "-o",extracted_bc_path, "--func", func_name])

    if template:
        subprocess.run([handsan_path, "--build", "-o", output_dir, extracted_bc_path, func_name])
    else:
        subprocess.run([handsan_path, "--build", "--no-template", "-o", output_dir, extracted_bc_path, func_name])

    output_obj_file_path = get_filepath_in_output_dir(output_dir, extracted_bc_path, ".o")
    subprocess.run(["llc-11", "-filetype=obj", extracted_bc_path, "-o", output_obj_file_path])

    func_exec_file_path = get_filepath_in_output_dir(output_dir, func_name, "")
    func_generated_cpp_file_path = get_filepath_in_output_dir(output_dir, func_name, ".cpp")

    #TODO turn this into a path
    argparse_include_path = dirname + "/../../vendor/include"
    subprocess.run(
        ["clang++", "-std=c++17", output_obj_file_path, func_generated_cpp_file_path, "-o",
         func_exec_file_path, "-I" + argparse_include_path])

    # subprocess.run(["rm", extracted_bc_path, output_obj_file_path])

    # TODO later
# def essence_build_from_spec(bc_file: str):
