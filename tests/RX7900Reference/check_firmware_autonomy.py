"""Check observed process absence; visual animation confirmation is separate."""
import argparse
import datetime
import json
import pathlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=pathlib.Path)
parser.add_argument('--since-marker', required=True)
parser.add_argument('--seconds', type=float, default=15)
args = parser.parse_args()
records = [json.loads(line) for line in (args.directory / 'raw.jsonl').read_text(encoding='utf-8').splitlines()]
marker = next(r for r in reversed(records) if r.get('marker') == args.since_marker)
inventories = [r for r in records if r.get('event') == 'process-inventory' and r['utc'] >= marker['utc']]
start = None
end = None
samples = 0
remaining = {}
for inventory in inventories:
    remaining = inventory['producers']
    if remaining:
        start = end = None
        samples = 0
    else:
        start = start or inventory['utc']
        end = inventory['utc']
        samples += 1
duration = (datetime.datetime.fromisoformat(end) - datetime.datetime.fromisoformat(start)).total_seconds() if start and end else 0
result = {'marker': args.since_marker, 'observed_empty_from': start, 'observed_empty_to': end,
          'duration_seconds': round(duration, 3), 'inventory_samples': samples, 'remaining_producers': remaining,
          'process_absence_at_least_required_duration': duration >= args.seconds,
          'visual_animation_confirmation': 'must be supplied by user separately',
          'limits': 'process inventory snapshots every 5 seconds; official PID changes also polled every 0.5 seconds; no kernel tracing'}
target = args.directory / (args.since_marker + '-process-evidence.json')
target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps(result, indent=2))
