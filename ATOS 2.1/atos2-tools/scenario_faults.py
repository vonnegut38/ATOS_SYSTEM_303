#!/usr/bin/env python3
"""68030 (CPU=3) run of the user-fault containment regression.

Kept as the historical entry point; the shared implementation lives in
scenario_faults_cpu.py and takes the CPU from the environment.
"""
import os, runpy, sys

here = os.path.dirname(os.path.abspath(__file__))
os.environ.setdefault("CPU", "3")
runpy.run_path(os.path.join(here, "scenario_faults_cpu.py"), run_name="__main__")
