import subprocess
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--foo', help='foo help')
args = parser.parse_args()
print("We now have this shit")
print(args.foo)



