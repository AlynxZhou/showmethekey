#!/usr/bin/env python3

import os
import sys
import subprocess

schema_dir = os.path.join(sys.argv[1], 'glib-2.0', 'schemas')
icon_dir = os.path.join(sys.argv[1], 'icons', 'hicolor')

# Package manager will set DESTDIR on packaging and run those on installing.
# We do nothing if packaging.
if not os.environ.get("DESTDIR"):
    argv = ["glib-compile-schemas", schema_dir]
    print(" ".join(argv))
    subprocess.call(argv)

    argv = ["gtk4-update-icon-cache", "-f", "-t", icon_dir]
    print(" ".join(argv))
    subprocess.call(argv)
