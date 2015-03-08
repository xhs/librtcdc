# setup.py
# Copyright (c) 2015 Xiaohan Song <chef@dark.kitchen>
# This file is licensed under a BSD license.

from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

setup(
  ext_modules = cythonize([Extension("pyrtcdc", ["pyrtcdc.pyx"], libraries=["rtcdc"])])
)
