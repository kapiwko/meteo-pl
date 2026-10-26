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
    return result

def main():
    app=shutil.which("meteo-pl")
    if not app or Path(app).resolve()!=Path("/usr/bin/meteo-pl").resolve():
        raise RuntimeError(f"Expected installed /usr/bin/meteo-pl, found {app}")
    widget=Path("/usr/share/plasma/plasmoids/pl.meteo.desktop/metadata.json")
    if not widget.is_file():raise RuntimeError("Installed Plasma widget is missing")
    with tempfile.TemporaryDirectory(prefix="meteo-installed-") as temporary:
        env=os.environ.copy()
        for name in ("QML_IMPORT_PATH","QML2_IMPORT_PATH","LD_LIBRARY_PATH","QT_PLUGIN_PATH"):env.pop(name,None)
        env.update(QT_QPA_PLATFORM="offscreen",QT_QUICK_BACKEND="software",QT_FORCE_STDERR_LOGGING="1",
            XDG_CONFIG_HOME=temporary+"/config",XDG_CACHE_HOME=temporary+"/cache",LC_ALL="C.UTF-8")
        expected=(Path(__file__).resolve().parents[1]/"VERSION").read_text().strip()
        if check([app,"--version"],env).stdout.strip() != "meteo-pl "+expected:
            raise RuntimeError("The installed application has an unexpected version")
        check([app,"--smoke-test"],env)
        check([sys.executable,str(Path(__file__).with_name("plasma-smoke.py")),str(widget.parent)],env)
        plugins = {p.resolve() for lib in (Path('/usr/lib'), Path('/usr/lib64'))
            for pattern in ('qt6/plugins/kf6/krunner/meteo_runner.so', '*/qt6/plugins/kf6/krunner/meteo_runner.so')
            for p in lib.glob(pattern)}
        if len(plugins) != 1: raise RuntimeError(f"Expected one installed KRunner plugin, found {plugins}")
        dependencies = check(["ldd",str(plugins.pop())],env)
        if 'not found' in dependencies.stdout: raise RuntimeError("An installed KRunner dependency is missing")
        check(["desktop-file-validate","/usr/share/applications/pl.meteo.desktop.desktop"],env)
    return 0
if __name__=="__main__":raise SystemExit(main())
