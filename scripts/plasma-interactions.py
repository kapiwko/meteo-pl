#!/usr/bin/env python3
"""Exercise the real widget in Plasma against an isolated local weather service."""
import argparse
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
ERRORS = re.compile(r'TypeError|ReferenceError|Binding loop|Cannot read property|Cannot assign|Error loading QML|is not a type|is not installed')


def scenario(name, args):
    with tempfile.TemporaryDirectory(prefix='meteo-plasma-interaction-') as directory:
        temporary = Path(directory)
        package = temporary / 'data/plasma/plasmoids/pl.meteo.desktop'
        shutil.copytree(ROOT / 'integrations/plasma/package', package)
        shutil.copy2(ROOT / 'tests/plasma/InteractionProbe.qml', package / 'contents/ui')
        main = package / 'contents/ui/main.qml'
        text = main.read_text()
        target = 'WeatherModel {\n        id: forecastModel'
        if text.count(target) != 1: raise RuntimeError('Cannot identify the widget weather model')
        text = text.replace(target, 'FixtureWeatherModel {\n        id: forecastModel', 1)
        text = text.replace('PlasmoidItem {', 'import pl.meteo.testfixtures\n\nPlasmoidItem {', 1)
        probe = '''
    InteractionProbe {
        applet: root
        weather: forecastModel
        configuration: Plasmoid.configuration
        actions: Plasmoid.contextualActions
        appletIcon: Plasmoid.icon
    }
'''
        position = text.rfind('}')
        main.write_text(text[:position] + probe + text[position:])
        if name == 'empty':
            config = package / 'contents/config/main.xml'
            config.write_text(config.read_text().replace('<default>Warszawa</default>', '<default></default>'))
        launch = temporary / 'bin/meteo-pl'
        launch.parent.mkdir()
        launch.write_text('#!/bin/sh\nprintf \'%s\\n\' "$@" > "$METEO_TEST_LAUNCH_OUTPUT"\n')
        launch.chmod(0o700)
        payload = temporary / 'payload.json'
        requests = []

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *unused): pass
            def respond(self):
                requests.append({'method': self.command, 'path': self.path})
                if name == 'loading': time.sleep(0.6)
                if name == 'ready' and self.command == 'POST' and sum(request['method'] == 'POST' for request in requests) > 1:
                    time.sleep(0.4)
                status = 503 if name in ('failed', 'cached') else 200
                if status != 200:
                    body = b'{}'
                elif self.command == 'GET':
                    forecast = json.loads(payload.read_text())
                    run = int(datetime.fromisoformat(forecast['fstart']).timestamp())
                    body = json.dumps({model: [run] for model in ('um4_60', 'um4_120', 'gfs')}).encode()
                else:
                    body = payload.read_bytes()
                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.end_headers()
                try: self.wfile.write(body)
                except (BrokenPipeError, ConnectionResetError): pass
            do_GET = respond
            def do_POST(self):
                self.rfile.read(int(self.headers.get('Content-Length', '0')))
                self.respond()

        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': 0.05}, daemon=True)
        thread.start()
        env = os.environ | {
            'XDG_DATA_HOME': str(temporary / 'data'), 'XDG_CONFIG_HOME': str(temporary / 'config'),
            'XDG_CACHE_HOME': str(temporary / 'cache'), 'QML_IMPORT_PATH': str(args.qml_path),
            'QT_QPA_PLATFORM': 'offscreen', 'QT_QUICK_BACKEND': 'software', 'QT_FORCE_STDERR_LOGGING': '1',
            'LANGUAGE': 'en_US', 'PATH': str(launch.parent) + os.pathsep + os.environ['PATH'],
            'METEO_TEST_FORECAST_ENDPOINT': f'http://127.0.0.1:{server.server_port}',
            'METEO_TEST_SCENARIO': name, 'METEO_TEST_PAYLOAD': str(payload),
            'METEO_TEST_SCREENSHOT': str(args.artifacts / f'{name}.png'),
            'METEO_TEST_LAUNCH_OUTPUT': str(temporary / 'launch.txt'),
        }
        command = [args.program, 'pl.meteo.desktop']
        if shutil.which('dbus-run-session'): command = ['dbus-run-session', '--', *command]
        log_path = args.artifacts / f'{name}.log'
        try:
            with log_path.open('w') as log:
                process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                try:
                    deadline = time.monotonic() + 15
                    while time.monotonic() < deadline:
                        output = log_path.read_text(errors='replace')
                        if 'METEO_TEST_PASS' in output or 'METEO_TEST_FAIL' in output or process.poll() is not None: break
                        time.sleep(0.05)
                finally:
                    try: os.killpg(process.pid, signal.SIGTERM)
                    except ProcessLookupError: pass
                    try: process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL); process.wait(timeout=3)
                    # D-Bus helpers can outlive the host briefly and keep writing
                    # into its isolated cache. Stop the whole test group before
                    # removing the temporary XDG directories.
                    try: os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError: pass
            output = log_path.read_text(errors='replace')
            if 'METEO_TEST_PASS' not in output or 'METEO_TEST_FAIL' in output or ERRORS.search(output):
                raise RuntimeError(f'{name}: widget interaction failed\n{output[-10000:]}')
            if name in ('ready', 'cached'):
                launched = temporary / 'launch.txt'
                for _ in range(20):
                    if launched.exists(): break
                    time.sleep(0.05)
                arguments = launched.read_text().splitlines()
                expected = 'Gdańsk' if name == 'ready' else 'Warszawa'
                if arguments[:2] != ['--location', expected] or '--model' not in arguments:
                    raise RuntimeError(f'{name}: incorrect application handoff: {arguments}')
            if name == 'empty' and requests: raise RuntimeError('Empty widget made a forecast request')
            (args.artifacts / f'{name}.json').write_text(json.dumps({'scenario': name, 'requests': requests, 'passed': True}, indent=2))
            print(f'{name}: passed ({len(requests)} local requests)', flush=True)
        finally:
            server.shutdown(); server.server_close(); thread.join(timeout=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--program', default='plasmawindowed')
    parser.add_argument('--qml-path', type=Path, required=True)
    parser.add_argument('--artifacts', type=Path, required=True)
    args = parser.parse_args()
    args.qml_path = args.qml_path.resolve(); args.artifacts = args.artifacts.resolve()
    args.artifacts.mkdir(parents=True, exist_ok=True)
    for name in ('empty', 'loading', 'failed', 'cached', 'ready'): scenario(name, args)


if __name__ == '__main__': main()
