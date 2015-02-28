#!/usr/bin/env bash

CFLAGS="-I../src" LDFLAGS="-L../src" python setup.py build_ext -i
