"""Compatibility entry point for the current missile defense runtime suite."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("missile_advance_runtime.py")),run_name="__main__")
