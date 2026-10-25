#!/usr/bin/env python3
"""Verify the installed app and Plasma widget without build-tree import paths."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import sys

ERRORS=re.compile(r"TypeError|ReferenceError|Binding loop|failed to load component|is not installed|is not a type|Invalid image provider|Unknown option|Error loading QML", re.IGNORECASE)

def check(command,env):
    print("[installed] "+" ".join(command),flush=True)
    result=subprocess.run(command,env=env,capture_output=True,text=True,timeout=20)
    print(result.stdout+result.stderr)
    if result.returncode or ERRORS.search(result.stdout+result.stderr):
        raise RuntimeError(f"Installed smoke test failed: {command}")

def main():
    app=shutil.which("meteo-pl")
    if not app or Path(app).resolve()!=Path("/usr/bin/meteo-pl").resolve():
        raise RuntimeError(f"Expected installed /usr/bin/meteo-pl, found {app}")
    widget=Path("/usr/share/plasma/plasmoids/pl.meteo.desktop/metadata.json")
    if not widget.is_file():raise RuntimeError("Installed Plasma widget is missing")
    with tempfile.TemporaryDirectory(prefix="meteo-installed-") as temporary:
        env=os.environ.copy()
        for name in ("QML_IMPORT_PATH","QML2_IMPORT_PATH","LD_LIBRARY_PATH","QT_PLUGIN_PATH"):env.pop(name,None)
        env.update(QT_QPA_PLATFORM="offscreen",QT_QUICK_BACKEND="software",QSG_RHI_BACKEND="software",
            XDG_CONFIG_HOME=temporary+"/config",XDG_CACHE_HOME=temporary+"/cache",LC_ALL="C.UTF-8")
        check([app,"--smoke-test"],env)
        check([sys.executable,str(Path(__file__).with_name("plasma-smoke.py")),"pl.meteo.desktop"],env)
        check(["desktop-file-validate","/usr/share/applications/pl.meteo.desktop.desktop"],env)
    return 0
if __name__=="__main__":raise SystemExit(main())
