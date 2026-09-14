"""Counterexamples that a benchmark must reject instead of reporting success."""

import copy
import json
from pathlib import Path
import unittest

from run import expected_fingerprint, fingerprints, validate_report
from sweep import summarize


class ValidationTest(unittest.TestCase):
    def setUp(self):
        self.expected = json.loads((Path(__file__).with_name('baseline') / 'expected.json').read_text())
        self.report = dict(profile='all', mode='verify', passes=2, threads=4,
                           dex_num=41, api_errors=0, create_ns=1, close_ns=1,
                           create_to_close_observed_ns=3, completed=True, stages=[], features=[])
        for number in range(2):
            for name in ['stages', 'features']:
                self.report[name].extend(dict(row, **{'pass': number})
                                         for row in copy.deepcopy(self.expected[name]))

    def test_complete_baseline_matches(self):
        validate_report(self.report, 'all', 'verify', 2, 4)
        wanted = expected_fingerprint(self.expected, 'all')
        self.assertTrue(all(value == wanted for value in fingerprints(self.report).values()))

    def test_truncated_second_pass_is_not_success(self):
        self.report['stages'] = [r for r in self.report['stages'] if r['pass'] == 0]
        with self.assertRaisesRegex(ValueError, 'passes'):
            validate_report(self.report, 'all', 'verify', 2, 4)

    def test_query_failure_is_not_an_expected_empty_result(self):
        self.report['api_errors'] = 1
        with self.assertRaisesRegex(ValueError, 'api_errors'):
            validate_report(self.report, 'all', 'verify', 2, 4)

    def test_missing_returned_empty_group_is_rejected(self):
        wanted = expected_fingerprint(self.expected, 'all')
        self.report['stages'][0]['returned_keys'] = []
        self.assertNotEqual(fingerprints(self.report)[0], wanted)

    def test_missing_lifecycle_and_empty_reports_are_rejected(self):
        del self.report['close_ns']
        with self.assertRaises(ValueError):
            validate_report(self.report, 'all', 'verify', 2, 4)
        with self.assertRaises(ValueError):
            validate_report({}, 'all', 'verify', 2, 4)

    def test_skipped_query_with_unchanged_final_selection_is_rejected(self):
        self.report['stages'].pop(1)
        self.assertNotEqual(fingerprints(self.report)[0], expected_fingerprint(self.expected, 'all'))

    def test_changed_order_or_multiplicity_is_rejected(self):
        self.report['stages'][1], self.report['stages'][2] = self.report['stages'][2], self.report['stages'][1]
        self.assertNotEqual(fingerprints(self.report)[0], expected_fingerprint(self.expected, 'all'))
        self.report['stages'][1]['count'] += 1
        self.assertNotEqual(fingerprints(self.report, True)[0], expected_fingerprint(self.expected, 'all', True))

    def test_timing_counts_do_not_replace_full_verification(self):
        self.report['stages'][1]['multiset_sha256'] = 'different'
        self.assertEqual(fingerprints(self.report, True)[0], expected_fingerprint(self.expected, 'all', True))
        self.assertNotEqual(fingerprints(self.report)[0], expected_fingerprint(self.expected, 'all'))

    def test_failed_memory_probe_is_not_a_memory_measurement(self):
        rows = [dict(pair=pair, label=label, peak_footprint_bytes=-1)
                for pair in range(2) for label in ['a', 'b']]
        self.assertNotIn('peak_footprint_bytes', summarize(rows, ['a', 'b'], 2, 42))


if __name__ == '__main__':
    unittest.main()
