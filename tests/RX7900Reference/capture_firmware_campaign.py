"""Watch official RGB Tool PIDs and observe ADL calls; never invoke device APIs.

Use --mark NAME --note TEXT from another command to label the active trace.
Startup calls before a PID is attached are explicitly outside capture coverage.
"""
import argparse
import ast
import datetime
import json
import pathlib
import sys
import threading
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--directory', type=pathlib.Path, required=True)
parser.add_argument('--tools', type=pathlib.Path)
parser.add_argument('--seconds', type=int, default=3600)
parser.add_argument('--mark')
parser.add_argument('--note', default='')
parser.add_argument('--stop', action='store_true', help='Request cooperative shutdown of an active campaign')
args = parser.parse_args()
args.directory.mkdir(parents=True, exist_ok=True)

def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

queue = args.directory / 'markers.jsonl'
stop_file = args.directory / 'stop.request'
if args.stop:
    stop_file.write_text(utc() + '\n', encoding='utf-8')
    print('Stop requested', flush=True)
    sys.exit(0)
if args.mark:
    with queue.open('a', encoding='utf-8') as file:
        file.write(json.dumps({'utc': utc(), 'marker': args.mark, 'note': args.note}) + '\n')
    print(args.mark, flush=True)
    sys.exit(0)

if args.tools:
    sys.path.insert(0, str(args.tools.resolve()))
import frida

# Reuse the observer's JS as data, without importing its CLI or executing it.
tree = ast.parse(pathlib.Path(__file__).with_name('capture_adl.py').read_text())
source = next(ast.literal_eval(node.value) for node in tree.body
              if isinstance(node, ast.Assign) and any(isinstance(target, ast.Name) and target.id == 'SOURCE' for target in node.targets))
source = source.replace("'ADL2_Display_WriteAndReadI2C'];", "'ADL2_Display_WriteAndReadI2C', 'ADL2_Display_WriteAndReadSMUI2C'];")
device = frida.get_local_device()
lock = threading.Lock()
sessions = {}
scripts = {}
known = {}
queue_offset = 0
last_inventory = 0
deadline = time.monotonic() + args.seconds

with (args.directory / 'raw.jsonl').open('a', encoding='utf-8', buffering=1) as output:
    def emit(record, show=False):
        with lock:
            output.write(json.dumps({'utc': utc(), **record}) + '\n')
        if show:
            try:
                print(json.dumps(record), flush=True)
            except BrokenPipeError:
                pass

    emit({'event': 'campaign-start', 'coverage': 'PID polling; startup before attach is not covered'}, True)
    try:
        while time.monotonic() < deadline and not stop_file.exists():
            processes = device.enumerate_processes()
            official = {p.pid: p.name for p in processes if p.name.lower().startswith('amd_radeon_rx_7900_xtx_series_rgb_led')}
            producers = {p.pid: p.name for p in processes if any(term in p.name.lower() for term in ['openrgb', 'chroma', 'synapse', 'masterplus', 'amd_radeon_rx_7900_xtx_series_rgb_led'])}
            for pid, name in official.items():
                if pid in sessions:
                    continue
                try:
                    session = device.attach(pid)
                    script = session.create_script(source)
                    script.on('message', lambda message, data, pid=pid: emit({'pid': pid, **message}))
                    session.on('detached', lambda *event_args, pid=pid: emit({'event': 'detached', 'pid': pid,
                        'reason': next((value for value in event_args if isinstance(value, str)), 'unknown')}, True))
                    sessions[pid] = session
                    scripts[pid] = script
                    script.load()
                    emit({'event': 'attached', 'pid': pid, 'name': name, 'coverage': 'startup before this event not covered'}, True)
                except Exception as error:
                    emit({'event': 'attach-error', 'pid': pid, 'error': str(error)}, True)
                    sessions.pop(pid, None)
                    scripts.pop(pid, None)
            for pid in set(known) - set(official):
                emit({'event': 'official-process-exited', 'pid': pid, 'name': known[pid]}, True)
                sessions.pop(pid, None)
                scripts.pop(pid, None)
            known = official
            if time.monotonic() - last_inventory >= 5:
                emit({'event': 'process-inventory', 'producers': producers, 'official': official})
                last_inventory = time.monotonic()
            if queue.exists():
                with queue.open(encoding='utf-8') as file:
                    file.seek(queue_offset)
                    for line in file:
                        emit({'event': 'marker', **json.loads(line)}, True)
                    queue_offset = file.tell()
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        for session in list(sessions.values()):
            try:
                session.detach()
            except frida.InvalidOperationError:
                pass
        emit({'event': 'campaign-stop'}, True)
