"""Summarize isolated Direct probe traces without contacting hardware."""
import argparse
import collections
import json
import math
import pathlib
import statistics


def stats(values):
    if not values:
        return None
    values = sorted(values)
    return dict(count=len(values), mean_ms=statistics.mean(values),
                p95_ms=values[math.ceil(len(values)*.95)-1], max_ms=values[-1])


def summarize(path):
    records = [json.loads(line) for line in path.read_text().splitlines()]
    starts = {r['id']: r for r in records if r['event'] == 'exchange-start'}
    ends = [r for r in records if r['event'] == 'exchange-end']
    transfers = collections.defaultdict(list)
    for record in records:
        if record['event'] == 'transfer':
            transfers[record['id']].append(record)
    invalid = []
    for end in ends:
        sequence = transfers[end['id']]
        message = starts[end['id']]['message']
        if not end['ok']:
            invalid.append(dict(id=end['id'], reason='probe-rejected'))
            continue
        if len(sequence) != 6 or any(r['result'] != 0 for r in sequence):
            invalid.append(dict(id=end['id'], reason='incomplete-or-ADL-error'))
            continue
        request = []
        reply = []
        for i, (size, wh, rh) in enumerate(zip((24, 24, 19), (64, 87, 174), (255, 232, 209))):
            write, read = sequence[i], sequence[i+3]
            if (not write['write'] or read['write'] or len(write['request']) != size
                    or len(read['response']) != size or write['request'][0] != wh
                    or read['response'][0] != rh):
                invalid.append(dict(id=end['id'], reason='frame-format'))
            request.extend(write['request'][1:])
            reply.extend(read['response'][1:])
        if request != message or (reply != message if message[:2] != [18, 2] else reply[:2] != [18, 2]):
            invalid.append(dict(id=end['id'], reason='payload'))
    frames = [r for r in records if r['event'] == 'frame']
    complete = next((r for r in records if r['event'] == 'complete'), None)
    configuration = next((r for r in records if r['event'] == 'start'), None)
    messages_per_frame = 2 if configuration and configuration['sequence']=='static-apply-mode-selected-once' else 3
    management = collections.defaultdict(list)
    for r in records:
        if r['event'] == 'management':
            management[r['api']].append(r)
    return dict(trace=str(path), configuration=configuration,
                complete=complete,
                effective_fps=complete['frames']*1000/complete['elapsed_ms'] if complete else None,
                animation_messages_per_second=complete['frames']*messages_per_frame*1000/complete['elapsed_ms'] if complete else None,
                frames=stats([r['leave_ms']-r['enter_ms'] for r in frames]),
                frame_start_intervals=stats([b['enter_ms']-a['enter_ms'] for a,b in zip(frames,frames[1:])]),
                exchanges=stats([r['duration_ms'] for r in ends]),
                transfers=stats([r['leave_ms']-r['enter_ms'] for seq in transfers.values() for r in seq]),
                waits=stats([r['leave_ms']-r['enter_ms'] for r in records if r['event']=='wait']),
                management={name:dict(timing=stats([r['leave_ms']-r['enter_ms'] for r in group]),
                                     results=dict(collections.Counter(r['result'] for r in group)))
                            for name, group in management.items()},
                invalid_exchanges=invalid, unfinished_exchange_ids=sorted(set(starts)-{r['id'] for r in ends}),
                visual_status='Requires separate human confirmation; ACKs do not prove smooth LED output.')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=pathlib.Path)
    args = parser.parse_args()
    print(json.dumps(summarize(args.trace), indent=2))
