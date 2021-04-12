#!/usr/bin/env python3

import os
import sys
import subprocess

if not os.environ.get("DESTDIR"):
    argv = ["glib-compile-schemas", sys.argv[1]]
    print(" ".join(argv))
    subprocess.call(argv)
