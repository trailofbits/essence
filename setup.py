#!/usr/bin/env python3

import os
import platform
import subprocess
import sys
import sysconfig
from pathlib import Path

from setuptools import Extension, find_packages, setup
from setuptools.command.build_ext import build_ext

module_name = "essence"


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
    entry_points={
        "console_scripts": [ "essence = essence.cli:essence" ]
    },
    url="https://github.com/trailofbits/essence",
    python_requires=">=3.7",
)
