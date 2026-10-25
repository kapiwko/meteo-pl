#!/usr/bin/env python3
"""Run Plasma's smoke mode, or briefly host the widget on older Plasma 6."""
import argparse
import re
import json
import os
from pathlib import Path
import shutil
import tempfile
import subprocess

ERRORS=re.compile(r"TypeError|ReferenceError|Binding loop|failed to load component|is not installed|is not a type|Invalid image provider|Unknown option|Error loading QML|Error loading applet|Could not load applet",re.IGNORECASE)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("applet")
    parser.add_argument("--program",default="plasmawindowed")
    args=parser.parse_args()
    help_result=subprocess.run([args.program,"--help"],capture_output=True,text=True,timeout=10)
    smoke="--smoke-test" in help_result.stdout
    environment=os.environ.copy()
    with tempfile.TemporaryDirectory(prefix="meteo-plasma-smoke-") as temporary:
        applet=args.applet
        package=Path(applet)
        if package.is_dir():
            applet=json.loads((package/"metadata.json").read_text())["KPlugin"]["Id"]
            shutil.copytree(package,Path(temporary)/"plasma/plasmoids"/applet)
            environment["XDG_DATA_HOME"]=temporary
        command=[args.program,*(["--smoke-test"] if smoke else []),applet]
        return run(command,environment,smoke)

def run(command,environment,smoke):
    process=subprocess.Popen(command,env=environment,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        output,_=process.communicate(timeout=12 if smoke else 4)
        code=process.returncode
    except subprocess.TimeoutExpired:
        process.terminate()
        try:output,_=process.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill();output,_=process.communicate()
        code=1 if smoke else 0
    print(output,end="")
    return 1 if code or ERRORS.search(output) else 0
if __name__=="__main__":raise SystemExit(main())
