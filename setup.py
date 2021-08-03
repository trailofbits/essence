#!/usr/bin/env python3

from setuptools import find_packages, setup

# version = {}
# with open("./src/blight/version.py") as f:
#     exec(f.read(), version)

# with open("./README.md") as f:
#     long_description = f.read()

setup(
    name="essence",
    version=0.1,
    license="Apache-2.0",
    author="Sebastiaan Peters",
    author_email="sebastiaan.peters@trailofbits.com",
    description="A tool to extract functions in a llvm bitcode module into their own ELF executable",
    # long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    package_data={
        "": "skelmain.*"
    },
    entry_points={
        "console_scripts": [
            "essence = essence.cli:essence",
            "essence-hello = essence.cli:hello"
            ]
    },
    url="https://github.com/trailofbits/handsanitizer",
    python_requires=">=3.7",
)
