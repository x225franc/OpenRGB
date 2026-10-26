"""Create summaries and byte manifests from completed evidence; no GPU access."""
import argparse
import datetime
import gzip
import hashlib
import json
import pathlib
import shutil

from analyze_direct_cadence import summarize

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=pathlib.Path)
parser.add_argument('--archive-long', action='store_true')
parser.add_argument('--manifest', action='store_true')
args = parser.parse_args()
results = []
for trace in sorted(args.directory.glob('native-*.jsonl')):
    if trace.name.endswith('.metrics.jsonl'):
        continue
    result = summarize(trace)
    results.append(result)
    if args.archive_long and result['complete'] and result['configuration']['seconds'] >= 300:
        with trace.open('rb') as source, gzip.open(str(trace)+'.gz', 'wb') as target:
            shutil.copyfileobj(source, target)
        # Originals stay local. Archive is byte-for-byte lossless after decompression.
        with gzip.open(str(trace)+'.gz','rb') as source:
            if hashlib.sha256(source.read()).digest() != hashlib.sha256(trace.read_bytes()).digest():
                raise RuntimeError('Compressed evidence mismatch')
(args.directory/'native-summaries.json').write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
for result in results:
    frame = result['frames']
    complete = result['complete']
    print(pathlib.Path(result['trace']).name,
          f"fps={result['effective_fps']:.3f}" if complete else 'INCOMPLETE',
          f"frame mean/p95/max={frame['mean_ms']:.3f}/{frame['p95_ms']:.3f}/{frame['max_ms']:.3f}" if frame else '',
          f"bad={len(result['invalid_exchanges'])} unfinished={len(result['unfinished_exchange_ids'])}")
if args.manifest:
    files = []
    for path in sorted(args.directory.rglob('*')):
        if not path.is_file() or path.name=='manifest.json':
            continue
        if args.archive_long and path.name.startswith('native-') and path.suffix=='.jsonl' and pathlib.Path(str(path)+'.gz').exists():
            continue
        data = path.read_bytes()
        files.append(dict(path=path.relative_to(args.directory).as_posix(), bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    (args.directory/'manifest.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),files=files),indent=2)+'\n',encoding='utf-8')
