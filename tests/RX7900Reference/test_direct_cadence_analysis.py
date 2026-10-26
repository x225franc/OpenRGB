"""Offline checks: reject corrupted evidence, never invoke GPU APIs."""
import json
import pathlib
import tempfile
import unittest

from analyze_direct_cadence import summarize

FIXTURE = pathlib.Path(__file__).parent / 'direct-cadence-20261026/native-2fps-30s-repeat.jsonl'


class EvidenceChecks(unittest.TestCase):
    def test_complete_trace(self):
        result = summarize(FIXTURE)
        self.assertEqual(result['frames']['count'], 59)
        self.assertEqual(result['invalid_exchanges'], [])
        self.assertEqual(result['unfinished_exchange_ids'], [])

    def test_corrupted_reply_is_rejected_even_if_probe_said_success(self):
        records = [json.loads(line) for line in FIXTURE.read_text().splitlines()]
        read = next(r for r in records if r['event']=='transfer' and not r['write'])
        read['response'][0] ^= 1
        with tempfile.TemporaryDirectory() as directory:
            trace = pathlib.Path(directory)/'corrupt.jsonl'
            trace.write_text(''.join(json.dumps(r)+'\n' for r in records))
            self.assertTrue(summarize(trace)['invalid_exchanges'])

    def test_missing_exchange_end_is_reported(self):
        lines = FIXTURE.read_text().splitlines()
        records = [json.loads(line) for line in lines]
        last = max(r['id'] for r in records if r['event']=='exchange-end')
        with tempfile.TemporaryDirectory() as directory:
            trace = pathlib.Path(directory)/'incomplete.jsonl'
            trace.write_text(''.join(json.dumps(r)+'\n' for r in records
                                    if not (r['event']=='exchange-end' and r['id']==last)))
            self.assertIn(last, summarize(trace)['unfinished_exchange_ids'])


if __name__ == '__main__':
    unittest.main()
