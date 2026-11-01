#!/usr/bin/env python3
"""Exercise an actual APK on an explicitly selected, disposable Android emulator.

This resets app data and permission state. Never target a physical phone.
Artifacts include the UI hierarchy and logcat, not personal application files.
"""
import argparse
import json
import hashlib
import shutil
import shlex
import tempfile
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

PACKAGE = "pl.meteo.desktop"
ACTIVITY = PACKAGE + "/org.qtproject.qt.android.bindings.QtActivity"


class Device:
    def __init__(self, adb, serial, artifacts):
        if not re.fullmatch(r"emulator-\d+", serial):
            raise ValueError("Use a disposable emulator serial, never a physical device")
        self.command = [adb, "-s", serial]
        self.artifacts = artifacts
        self.pids = set()
        artifacts.mkdir(parents=True, exist_ok=True)
        (artifacts / "result.json").unlink(missing_ok=True)

    def run(self, *arguments):
        arguments=tuple(map(str,arguments))
        if arguments and arguments[0]=="shell": arguments=("shell",shlex.join(arguments[1:]))
        command=[*self.command,*arguments]
        for attempt in range(3):
            result=subprocess.run(command,capture_output=True,text=True,timeout=45)
            if result.returncode==0: return result.stdout
            if attempt<2 and re.search(r"device.*(?:offline|not found)|transport.*closed",result.stderr):
                time.sleep(2)
                continue
            raise subprocess.CalledProcessError(result.returncode,command,result.stdout,result.stderr)

    def hierarchy(self):
        self.run("shell", "uiautomator", "dump", "/sdcard/meteo-runtime.xml")
        xml = self.run("shell", "cat", "/sdcard/meteo-runtime.xml")
        (self.artifacts / "last-ui.xml").write_text(xml)
        return list(ET.fromstring(xml).iter("node"))

    def wait(self, predicate, description, seconds=90):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(.5)
        raise AssertionError(description)

    def matches(self, labels=(), resource="", prefix=False, action=False):
        resources, texts = [], []
        for node in self.hierarchy():
            a = node.attrib
            text = a.get("text") or a.get("content-desc") or ""
            bounds=list(map(int,re.findall(r"\d+",a.get("bounds",""))))
            if len(bounds)!=4 or bounds[2]<=bounds[0] or bounds[3]<=bounds[1]: continue
            if resource and a.get("resource-id","").endswith(resource): resources.append((a,bounds))
            if any(text.startswith(label) if prefix else text==label for label in labels): texts.append((a,bounds))
        candidates=resources or texts
        if action:
            candidates=[item for item in candidates if item[0].get("enabled")!="false"]
            buttons=[item for item in candidates if item[0].get("clickable")=="true" and item[0].get("enabled")=="true"]
            candidates=buttons or candidates
        # Qt exposes the obscured page too. Modal controls occur after it.
        return candidates[-1][1] if candidates else None

    def tap(self, *labels, resource="", prefix=False):
        bounds=None
        def locate():
            nonlocal bounds
            bounds=self.matches(labels,resource,prefix,action=True)
            return bounds
        self.wait(locate,"Missing action: " + repr(labels or resource))
        x,y,right,bottom=bounds
        self.run("shell", "input", "tap", (x+right)//2, (y+bottom)//2)
        time.sleep(1)

    def shown(self, *labels, prefix=False):
        self.wait(lambda: self.matches(labels, prefix=prefix), "Missing content: " + repr(labels),seconds=120)

    def edit(self, resource, text):
        print("Editing " + resource,flush=True)
        dimensions=list(map(int,re.findall(r"\d+",self.run("shell","wm","size"))))[-2:]
        width,height=dimensions
        for _ in range(6):
            if self.matches(resource=resource): break
            self.run("shell","input","swipe",width//2,int(height*.8),width//2,int(height*.3),350)
        self.tap(resource=resource)
        self.run("shell","input","text",text)
        # Hardware-keyboard emulators do not always show an IME; Back must
        # hide the keyboard only, otherwise it would leave the editor.
        ime=self.run("shell","dumpsys","input_method")
        if re.search(r"mInputShown=true",ime):
            time.sleep(.5)
            self.back()

    def foreground(self):
        return bool(re.search(r"(?:mResumedActivity|topResumedActivity|mFocusedApp)[:=][^\n]*" + re.escape(PACKAGE),
                              self.run("shell", "dumpsys", "activity", "activities")))

    def start(self):
        self.run("shell", "am", "start", "-W", "-n", ACTIVITY)
        self.pids.update(self.run("shell", "pidof", PACKAGE).split())
        # am start waits for the Java window, while Qt still incubates QML.
        # Query accessibility after that first construction has settled.
        time.sleep(10)

    def back(self):
        self.run("shell", "input", "keyevent", "KEYCODE_BACK")
        time.sleep(.5)


def exercise(device, apk):
    d = device
    abi=d.run("shell","getprop","ro.product.cpu.abi").strip()
    if abi=="x86_64" and "aes" not in d.run("shell","cat","/proc/cpuinfo").split():
        raise RuntimeError("ARM64 translation needs AES: start the emulator with -qemu -cpu host")
    for setting in ("window_animation_scale","transition_animation_scale","animator_duration_scale"):
        d.run("shell","settings","put","global",setting,"0")
    if PACKAGE in d.run("shell", "pm", "list", "packages", PACKAGE): d.run("uninstall", PACKAGE)
    d.run("install", "-r", apk)
    d.run("shell", "pm", "clear", PACKAGE)
    d.run("logcat", "-c")
    print("APK installed; checking first launch",flush=True)
    d.start()
    d.shown("Welcome to Meteo", "Witaj w Meteo")
    for step in range(2,5):
        d.tap("Next", "Dalej")
        d.shown(f"Step {step} of 4", f"Krok {step} z 4")
    d.tap("Show forecast", "Pokaż prognozę")
    # The real Android permission dialog, not a mocked Qt permission object.
    d.tap(resource="permission_deny_button")
    d.shown("Find a place", "Znajdź miejsce")
    print("Native permission refusal and fallback passed",flush=True)
    d.tap("Coordinates", "Współrzędne")
    d.edit("customLocationName","RuntimeKielce")
    d.edit("manualLatitude","50.8661")
    d.edit("manualLongitude","20.6286")
    d.tap("Add","Dodaj",resource="mobileSavePlaceButton")
    d.tap("More options", "Więcej opcji")
    d.tap("Settings", "Ustawienia")
    d.run("shell", "input", "keyevent", "KEYCODE_HOME")
    d.start()
    d.shown("Settings", "Ustawienia")
    d.back()
    assert d.foreground(), "Back from the editor closed the application"
    print("Background/foreground and system Back passed",flush=True)
    # Native navigation must survive configuration changes and an APK update.
    d.run("shell", "settings", "put", "system", "accelerometer_rotation", "0")
    d.run("shell", "settings", "put", "system", "user_rotation", "1")
    time.sleep(1)
    assert d.foreground(), "Rotation closed the application"
    d.run("shell", "settings", "put", "system", "user_rotation", "0")
    d.run("shell", "am", "force-stop", PACKAGE)
    d.run("install", "-r", apk)
    d.start()
    # Completed setup must not reappear after upgrading the same signed APK.
    d.wait(lambda: d.foreground(), "Updated APK did not launch")
    assert not d.matches(("Welcome to Meteo", "Witaj w Meteo")), "Update lost setup preferences"
    d.tap("More options", "Więcej opcji")
    d.tap("My places", "Moje miejsca")
    d.shown("RuntimeKielce",prefix=True)
    d.back()
    print("Rotation and APK update preserved the saved place",flush=True)
    d.tap("More options", "Więcej opcji", "Open menu", "Otwórz menu")
    d.tap("Settings", "Ustawienia")
    d.tap("Profiles", "Profile", prefix=True)
    # Export through the actual Android document provider (content://), then
    # inspect the resulting JSON. This catches local-path-only save regressions.
    d.tap("Export…", "Eksportuj…")
    d.tap("Export…", "Eksportuj…")
    filename="meteo-runtime-"+str(time.time_ns())+".json"
    d.edit("android:id/title",filename)
    d.tap("Save", "SAVE", "Zapisz", "ZAPISZ", resource="button1")
    path="/sdcard/Download/"+filename
    d.wait(lambda: path in d.run("shell", "find", "/sdcard/Download", "-name", "*.json").splitlines(), "SAF did not create the requested document")
    document = json.loads(d.run("shell", "cat", path))
    assert document.get("profiles"), "SAF export has no profiles"
    print("SAF profile export passed",flush=True)
    d.tap("Import…", "Importuj…")
    d.tap(filename,prefix=True)
    d.shown("Profiles imported: ", "Zaimportowane profile: ", "Zaimportowano profili: ",prefix=True)
    print("SAF profile import passed",flush=True)
    d.back()
    d.back()
    assert d.foreground(), "Back from settings closed the application"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--adb", default="adb")
    parser.add_argument("--serial", required=True)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--artifacts", type=Path, default=Path("build/android-runtime"))
    args = parser.parse_args()
    device = Device(args.adb, args.serial, args.artifacts)
    try:
        with tempfile.TemporaryDirectory(prefix="meteo-runtime-apk-") as temporary:
            snapshot=Path(temporary)/args.apk.name
            shutil.copy2(args.apk,snapshot)
            digest=hashlib.sha256(snapshot.read_bytes()).hexdigest()
            exercise(device,snapshot)
    finally:
        try: log = device.run("logcat", "-d")
        except subprocess.SubprocessError as error: log = "Logcat unavailable: " + str(error)
        (args.artifacts / "logcat.txt").write_text(log)
    application_log="\n".join(line for line in log.splitlines() if len(line.split())>3 and line.split()[2] in device.pids)
    if re.search(r"FATAL EXCEPTION|Fatal signal|TypeError:|ReferenceError:", application_log):
        raise AssertionError("Runtime error in Android logcat; see artifacts")
    (args.artifacts / "result.json").write_text(json.dumps({"apk":str(args.apk), "sha256":digest, "result":"passed"}, indent=2)+"\n")


if __name__ == "__main__":
    main()
