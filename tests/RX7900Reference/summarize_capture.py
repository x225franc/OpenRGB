"""Reassemble observed SMU write frames without contacting hardware."""
import collections
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
pending = {}
messages = []
results = collections.Counter()
for line in path.read_text(encoding='utf-8').splitlines():
    record = json.loads(line)
    payload = record.get('payload', {})
    if payload.get('event') == 'leave':
        results[payload['result']] += 1
    if payload.get('event') != 'enter':
        continue
    transaction = payload['i2c']
    if transaction['action'] != 2:
        continue
    data = transaction['data']
    key = (record.get('pid'), payload['adapter'])
    if len(data) == 24 and data[0] == 0x40:
        pending[key] = [record['utc'], data[1:]]
    elif key in pending and len(data) == 24 and data[0] == 0x57 and len(pending[key][1]) == 23:
        pending[key][1].extend(data[1:])
    elif key in pending and len(data) == 19 and data[0] == 0xAE and len(pending[key][1]) == 46:
        time, message = pending.pop(key)
        message.extend(data[1:])
        messages.append({'utc': time, 'pid': key[0], 'adapter': key[1],
                         'hex': bytes(message).hex(' ').upper()})

print(json.dumps({'return_codes': dict(results), 'messages': messages}, indent=2))
