# SPDX-License-Identifier: CC0-1.0
"""Run a code generator script from its source file, so its path-relative
imports and templates resolve, with this binary's bundled libraries."""

import os
import runpy
import sys

script = os.path.abspath(sys.argv[1])
sys.argv = [script] + sys.argv[2:]
sys.path.insert(0, os.path.dirname(script))
runpy.run_path(script, run_name='__main__')
