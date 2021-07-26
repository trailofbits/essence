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
    subprocess.run(["clang++", "-std=c++17", "../skelmain.cpp", target + ".o", "-I../", target + ".cpp", "-o", target])
    return subprocess.run(["./" + target, "-i", function_to_test + ".json"], capture_output=True).stdout


def test_scalar_assignments():
    program_output = call_handsanitizer("scalar_assignment")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
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
    program_output = call_handsanitizer("direct_struct_return_test")

    output_json = json.loads(program_output)
    print(json.dumps(output_json, indent=4))
    assert output_json["output"]["a0"] == 8589934593
    assert output_json["output"]["a1"] == 97
