"""Decode recorded firmware-effect observations; never contact hardware."""
import argparse
import bisect
import collections
import datetime
import json
import pathlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=pathlib.Path)
args = parser.parse_args()
records = [json.loads(line) for line in (args.directory / 'raw.jsonl').read_text(encoding='utf-8').splitlines()]
markers = sorted((r for r in records if r.get('event') == 'marker'), key=lambda r: r['utc'])
times = [r['utc'] for r in markers]
calls = {}
ordered = []
for record in records:
    payload = record.get('payload', {})
    key = (record.get('pid'), payload.get('id'))
    if payload.get('event') == 'enter':
        call = {'pid': key[0], 'id': key[1], 'api': payload['api'], 'adapter': payload['adapter'],
                'enter_utc': record['utc'], 'request': payload['i2c']}
        calls[key] = call
        ordered.append(call)
    elif payload.get('event') == 'leave' and key in calls:
        calls[key].update({'leave_utc': record['utc'], 'result': payload['result'], 'response': payload['i2c']})

pending = {}
messages = []
unassembled = 0

def label(utc):
    index = bisect.bisect_right(times, utc) - 1
    return markers[index]['marker'] if index >= 0 else 'unlabelled'

for call in ordered:
    request = call['request']
    data = request['data']
    key = (call['pid'], call['adapter'], call['api'])
    if request['action'] == 2 and len(data) == 24 and data[0] == 0x40:
        pending[key] = {'utc': call['enter_utc'], 'pid': key[0], 'adapter': key[1], 'api': key[2],
                        'label': label(call['enter_utc']), 'data': data[1:], 'transactions': [call]}
    elif key in pending and request['action'] == 2:
        message = pending[key]
        expected = (0x57, 24) if len(message['data']) == 23 else (0xAE, 19)
        if (data[0] if data else None, len(data)) != expected:
            unassembled += 1
            pending.pop(key)
            continue
        message['data'].extend(data[1:])
        message['transactions'].append(call)
        if len(message['data']) == 64:
            message['hex'] = bytes(message.pop('data')).hex(' ').upper()
            message['opcode'] = message['hex'][:5]
            messages.append(message)
    elif key in pending and request['action'] == 1:
        message = pending[key]
        if 'hex' not in message:
            unassembled += 1
            continue
        message['transactions'].append(call)
        if len(message['transactions']) == 6:
            pending.pop(key)

for message in messages:
    transactions = message['transactions']
    message['all_adl_ok'] = all(t.get('result') == 0 for t in transactions)
    message['complete_reply'] = len(transactions) == 6
    message['reply_hex'] = []
    if message['complete_reply']:
        message['reply_hex'] = [bytes(t.get('response', {}).get('data', [])).hex(' ').upper() for t in transactions[3:]]
        message['reply_markers_valid'] = [t.get('response', {}).get('data', [None])[0] for t in transactions[3:]] == [0xFF, 0xE8, 0xD1]
        reply = sum((t.get('response', {}).get('data', [])[1:] for t in transactions[3:]), [])
        message['reply_equals_message'] = bytes(reply).hex(' ').upper() == message['hex']
    message['transaction_gaps_ms'] = []
    for previous, current in zip(transactions, transactions[1:]):
        end = previous.get('leave_utc', previous['enter_utc'])
        message['transaction_gaps_ms'].append(round((datetime.datetime.fromisoformat(current['enter_utc']) - datetime.datetime.fromisoformat(end)).total_seconds() * 1000, 3))

legacy_pids = {m['pid'] for m in messages if m['api'] == 'ADL_Display_WriteAndReadSMUI2C'}
for message in messages:
    message['canonical'] = message['api'] == 'ADL_Display_WriteAndReadSMUI2C' if message['pid'] in legacy_pids else True
canonical = [m for m in messages if m['canonical']]
summary = {'messages': len(messages), 'canonical_messages': len(canonical),
           'messages_by_api': dict(collections.Counter(m['api'] for m in messages)),
           'counting_note': 'This driver forwards legacy SMU calls through ADL2; canonical intervals count only the legacy observation when both exist for a PID.',
           'adl_calls': len(ordered), 'unassembled': unassembled,
           'return_codes': dict(collections.Counter(str(c.get('result')) for c in ordered)),
           'failed_calls_by_adapter': dict(collections.Counter(str(c['adapter']) for c in ordered if c.get('result') != 0)),
           'capture_error_scope': 'Only errors serialized into JSONL. Host callback stderr exceptions are documented separately in capture-notes.json.',
           'capture_errors': [r for r in records if r.get('type') == 'error' or r.get('event') == 'attach-error'
                              or r.get('payload', {}).get('event') == 'decode-error'], 'intervals': []}
for index, marker in enumerate(markers):
    end = markers[index + 1]['utc'] if index + 1 < len(markers) else records[-1]['utc']
    group = [m for m in canonical if marker['utc'] <= m['utc'] < end]
    summary['intervals'].append({'marker': marker['marker'], 'note': marker.get('note', ''), 'start': marker['utc'], 'end': end,
                                 'messages': len(group), 'opcodes': dict(collections.Counter(m['opcode'] for m in group)),
                                 'configuration_prefixes': sorted({m['hex'][:47] for m in group if m['opcode'] == '51 2C'})})
(args.directory / 'messages.json').write_text('[\n' + ',\n'.join(json.dumps(message, separators=(',', ':')) for message in messages) + '\n]\n', encoding='utf-8')
(args.directory / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
print(json.dumps(summary, indent=2))
