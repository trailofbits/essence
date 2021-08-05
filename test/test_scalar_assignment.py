import os.path
import subprocess
import argparse
import json
from itertools import groupby
from pathlib import Path

Path("output").mkdir(parents=True, exist_ok=True)


# same input for all is name of bc

def call_handsanitizer(function_to_test):
    bc_file = function_to_test + ".bc"
    output_dir = "output/"
    target = output_dir + function_to_test

    subprocess.run(["../HandSanitizer", bc_file, "-g", "-o", output_dir])
    subprocess.run(["llc", "-filetype=obj", bc_file, "-o", target + ".o"])
    subprocess.run(["../HandSanitizer", bc_file, "-o", "output", function_to_test])
    subprocess.run(["clang++", "-g", "-std=c++17", target + ".o", target + ".cpp", "-o", target])
    return subprocess.run(["./" + target, "-i", function_to_test + ".json"], capture_output=True).stdout


def test_scalar_assignments():
    program_output = call_handsanitizer("scalar_assignment_tests")

    output_json = json.loads(program_output)

    assert output_json["globals"]["a"] == 91
    assert output_json["globals"]["b"] == 92
    assert output_json["globals"]["c"] == 93
    assert output_json["globals"]["d"] == 94
    assert output_json["globals"]["e"] == 5.199999809265137  # fp conversion bs
    assert output_json["globals"]["aa"] == 6.2


def test_structures():
    program_output = call_handsanitizer("full_struct_tests")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["a"]["a0"][0] == 1
    assert output_json["globals"]["a"]["a0"][1] == 2
    assert output_json["globals"]["a"]["a0"][2] == 3
    assert output_json["globals"]["a"]["a1"] == 4
    assert output_json["globals"]["a"]["a2"]["a0"] == 5
    assert output_json["globals"]["a"]["a2"]["a1"]["a0"] == 2
    # assert output_json["output"] == 23909216 # should find something better for this, but make sure its a pointer due to sret


def test_direct_return_of_structures():
    program_output = call_handsanitizer("direct_struct_return_tests")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["output"]["a0"] == 8589934593
    assert output_json["output"]["a1"] == 97


def test_pointer_array_non_null():
    program_output = call_handsanitizer("pointer_array_assignment_not_null_terminated_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["third_char_of_non_null_terminated_array"] != 0


def test_pointer_array_assignment_null_terminated():
    program_output = call_handsanitizer("pointer_array_assignment_null_terminated_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["second_char_of_array"] == 98  # b
    assert output_json["globals"]["third_char_of_array"] == 99  # c
    assert output_json["globals"]["fifth_char_of_array"] == 0
    assert output_json["globals"]["sixth_char_of_array"] == 101  # e
    assert output_json["globals"]["seventh_char_of_array"] == 102  # f
    assert output_json["globals"]["eight_char_of_array"] == 0


def test_pointer_array_of_ints():
    program_output = call_handsanitizer("pointer_array_of_ints_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["int_pointer_val_arg_second_value_in_array"] == 1


def test_pointer_direct_byte_assignment():
    program_output = call_handsanitizer("pointer_direct_byte_assignment_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["direct_char_value_number"] == 80


def test_pointer_direct_string_assignment():
    program_output = call_handsanitizer("pointer_direct_string_assignment_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["second_char_value_string"] == 98
    assert output_json["globals"]["third_char_of_string"] == 0


def test_pointer_global_assignment():
    program_output = call_handsanitizer("pointer_global_assignment_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["globals"]["int_pointer_global_val_post_call"] == 1
