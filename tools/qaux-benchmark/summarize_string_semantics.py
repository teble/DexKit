#!/usr/bin/env python3
"""Summarize witnesses from audit_string_semantics.py; no timing claims."""

import argparse
import base64
from collections import Counter
import json
from pathlib import Path

from audit_string_semantics import sha256


# These are the unchanged ordinary matcher conditions in QueryReplay.chains.
ORDINARY = {
    ('AIOMsgItem_initContentDescription', 'find_seed'): ['senderUid', 'peerUid'],
    ('EmotionDetailAi', 'find_classes'): ['MsgEmoticonPreviewData', 'doRestoreSaveInstanceState'],
    ('Hd_HideEmoReplyLayout_Method', 'find_classes'): ['AIOReceiverBubbleTemplate', 'msgTailContainer'],
    ('Hd_HideEmoReplyLayout_Method', 'find_method_in_classes'): ['msgTailContainer'],
    ('BlockPicByMd5_LoadImagePathV2', 'find_method'): ['picView', 'imagePath', 'msgElement', 'msgItem', 'loadingImage'],
    ('ReplyNoAtHook', 'reply_new_signature'): ['mContext', 'senderUid'],
    ('ReplyNoAtHook', 'reply_middle_signature'): ['mContext', 'senderUid'],
    ('ReplyNoAtHook', 'reply_old_signature'): ['msgItem.msgRecord.senderUid'],
    ('AutoReceiveOriginalPhoto_cache_miss', 'nt_on_init_view'): ['rootView', 'em_bas_view_the_original_picture'],
}


def normalized(raw):
    start, end = raw.startswith('^'), raw.endswith('$')
    value = raw[1:] if start else raw
    value = value[:-1] if end else value
    mode = 'Equals' if start and end else 'StartsWith' if start else 'EndsWith' if end else 'Contains'
    return value, mode


def matches(value, pattern, mode):
    if mode == 'Equals':
        return value == pattern
    if mode == 'StartsWith':
        return value.startswith(pattern)
    if mode == 'EndsWith':
        return value.endswith(pattern)
    return pattern in value


def coverage(total, equal, prefix):
    return ('no_candidates' if not total else 'equal_sufficient' if equal == total
            else 'prefix_sufficient' if prefix == total else 'contains_required')


def inspect(rows, patterns):
    atoms = []
    equal_all = [True] * len(rows)
    prefix_all = [True] * len(rows)
    for value, mode in patterns:
        # Current corpus patterns are nonempty BMP strings without embedded NUL.
        # No implicit Unicode conversion or case-folding is modeled here.
        assert value and all(0 < ord(c) < 0xD800 or 0xDFFF < ord(c) < 0x10000 for c in value)
        eq_n = pre_n = 0
        reference_kinds = Counter()
        samples = {'equal': [], 'proper_prefix': [], 'nonprefix': []}
        for index, row in enumerate(rows):
            strings = row['strings']
            assert any(matches(s, value, mode) for s in strings), (row['identity'], value, mode)
            eq, pre = value in strings, any(s.startswith(value) for s in strings)
            eq_n += eq
            pre_n += pre
            equal_all[index] &= eq
            prefix_all[index] &= pre
            for string in strings:
                if value not in string:
                    continue
                kind = 'equal' if string == value else 'proper_prefix' if string.startswith(value) else 'nonprefix'
                reference_kinds[kind] += 1
                sample = {'length': len(string), 'excerpt': string[:160], 'truncated': len(string) > 160}
                if sample not in samples[kind] and len(samples[kind]) < 3:
                    samples[kind].append(sample)
        atoms.append({'pattern': value, 'original_mode': mode, 'candidates': len(rows),
                      'equal_covered_candidates': eq_n, 'prefix_covered_candidates': pre_n,
                      'coverage': coverage(len(rows), eq_n, pre_n),
                      'matched_getter_entry_kinds': dict(reference_kinds), 'examples': samples})
    return {'candidates': len(rows), 'equal_covered_candidates': sum(equal_all),
            'prefix_covered_candidates': sum(prefix_all),
            'coverage': coverage(len(rows), sum(equal_all), sum(prefix_all)), 'atoms': atoms}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Choose a new output file.')
    root = Path(__file__).resolve().parent
    manifest = json.loads((args.run / 'manifest.json').read_text())
    reports = {}
    for mode, expected in manifest['outputs'].items():
        path = args.run / f'{mode}.json'
        assert sha256(path) == expected, path
        reports[mode] = json.loads(path.read_text())
    groups_path = root / 'baseline/groups.tsv'
    assert sha256(groups_path) in manifest['pinned_inputs'].values()
    groups = {}
    for line in groups_path.read_text().splitlines():
        fields = line.split('\t')
        groups[fields[0]] = [normalized(base64.b64decode(s).decode()) for s in fields[1:]]
    source_targets = json.loads((root / 'baseline/targets.json').read_text())
    source_urls = {t['name']: t['source_url'] for t in source_targets['targets']}
    base = reports['Contains']
    batch_stage = next(s for s in base['stages'] if s['stage'] == 'batch_strings')
    batch = []
    for key, patterns in groups.items():
        result = inspect(batch_stage['groups'][key]['audit_witnesses'], patterns)
        result.update(group=key, source_url=source_urls[key.split('#_#')[0]])
        batch.append(result)
    ordinary = []
    seen = set()
    for stage in base['stages']:
        key = (stage['feature'], stage['stage'])
        if key not in ORDINARY:
            continue
        seen.add(key)
        result = inspect(stage['audit_witnesses'], [(p, 'Contains') for p in ORDINARY[key]])
        result.update(feature=key[0], stage=key[1])
        result['ordered_results_equal'] = {}
        for mode in ('Equals', 'StartsWith'):
            matches_stage = [s for s in reports[mode]['stages'] if (s['feature'], s['stage']) == key]
            result['ordered_results_equal'][mode] = bool(matches_stage) and (
                matches_stage[0]['ordered_results'] == stage['ordered_results'])
        ordinary.append(result)
    summary = {
        'scope': 'QQ 9.3.55 only, all raw batch candidates and executed ordinary matcher results. No timing conclusions.',
        'interpretation': 'Coverage means each existing candidate has a sufficient witness; it is not caller intent, cross-version proof, whole-pool prevalence, or a runtime cost share.',
        'entries_note': 'Getter entries preserve returned multiplicity; these are not instruction-execution counts.',
        'empty_note': 'Empty-result conditions supply no positive witness evidence and are not counted as narrowing opportunities.',
        'manifest_sha256': sha256(args.run / 'manifest.json'),
        'summary_script_sha256': sha256(Path(__file__)),
        'diagnostic_contains_matches_frozen_order_and_flow': manifest['diagnostic_contains_matches_frozen_order_and_flow'],
        'ordinary_narrowing_same_order_and_flow': manifest['ordinary_narrowing_same_order_and_flow'],
        'ordinary_executed': len(ordinary),
        'ordinary_nonempty': sum(bool(row['candidates']) for row in ordinary),
        'ordinary_nonempty_distinct_patterns': sorted({a['pattern'] for row in ordinary if row['candidates'] for a in row['atoms']}),
        'ordinary_unexecuted': [{'feature': f, 'stage': s} for f, s in ORDINARY if (f, s) not in seen],
        'ordinary': ordinary,
        'batch_group_coverage': dict(Counter(row['coverage'] for row in batch)),
        'batch_contains_atom_coverage': dict(Counter(a['coverage'] for row in batch for a in row['atoms'] if a['original_mode'] == 'Contains')),
        'batch_groups': batch,
    }
    assert len(batch) == 149
    assert sum(summary['batch_contains_atom_coverage'].values()) == 183
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(summary, indent=2, ensure_ascii=True) + '\n')
    print(json.dumps({k: summary[k] for k in ('ordinary_executed', 'ordinary_nonempty',
          'batch_group_coverage', 'batch_contains_atom_coverage')}, indent=2))


if __name__ == '__main__':
    main()
