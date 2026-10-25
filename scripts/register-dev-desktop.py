#!/usr/bin/env python3
"""Register the development executable and icon with the user's desktop/portal."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

repository = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", type=Path, default=repository / "build/dev")
args = parser.parse_args()
binary = (args.build_dir / "apps/meteo/meteo-pl").resolve()
if not binary.is_file():
    parser.error(f"Najpierw zbuduj aplikację: brak {binary}")
data = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
desktop = data / "applications/pl.meteo.desktop.desktop"
if desktop.exists() and "X-Meteo-Development=true" not in desktop.read_text():
    parser.error(f"Istniejący wpis aplikacji pozostawiono bez zmian: {desktop}")
icon = data / "icons/hicolor/scalable/apps/pl.meteo.desktop.svg"
icon.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(repository / "integrations/plasma/package/contents/images/meteo.svg", icon)
notification = data / "knotifications6/pl.meteo.desktop.notifyrc"
notification.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(repository / "apps/meteo/pl.meteo.desktop.notifyrc", notification)
entry = (repository / "apps/meteo/pl.meteo.desktop.desktop").read_text()
quoted = '"' + str(binary).replace("\\", "\\\\").replace('"', '\\"').replace("`", "\\`").replace("$", "\\$") + '"'
entry = entry.replace("Exec=meteo-pl", "Exec=" + quoted) + "\nX-Meteo-Development=true\n"
desktop.parent.mkdir(parents=True, exist_ok=True)
temporary = desktop.with_suffix(".tmp")
temporary.write_text(entry)
temporary.replace(desktop)
for command in (["update-desktop-database", str(desktop.parent)], ["gtk-update-icon-cache", "-f", "-t", str(data / "icons/hicolor")], ["kbuildsycoca6", "--noincremental"]):
    if shutil.which(command[0]):
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            print(f"{command[0]}: {result.stderr.strip()}")
print(f"Zarejestrowano aplikację: {desktop}")
print(f"Ikona: {icon}")
print(f"Powiadomienia: {notification}")
