#!/usr/bin/env python3
"""Run balanced, paired fresh-process measurements of two verified binaries."""

import argparse
import json
from pathlib import Path
import random
import statistics
import subprocess
import sys


def percentile(values, fraction):
    ordered = sorted(values)
    point = (len(ordered) - 1) * fraction
    lower = int(point)
    return ordered[lower] + (ordered[min(lower + 1, len(ordered) - 1)] - ordered[lower]) * (point - lower)


def summarize(rows, labels, pairs, seed):
    metrics = ['lifecycle_ms', 'create_ms', 'close_ms', 'pass0_api_ms', 'pass1_api_ms',
               'repeated_api_ms', 'max_rss_bytes', 'window_peak_rss_bytes',
               'peak_footprint_bytes', 'window_peak_footprint_bytes']
    result = {}
    for metric in metrics:
        if any(row.get(metric) is None or row.get(metric) == 0 for row in rows):
            continue
        grouped = {label: [r[metric] for r in rows if r['label'] == label] for label in labels}
        stats = {}
        for label, values in grouped.items():
            median = statistics.median(values)
            stats[label] = dict(median=median, minimum=min(values), maximum=max(values),
                               mad=statistics.median(abs(v - median) for v in values))
        changes = []
        for pair in range(pairs):
            pair_rows = {r['label']: r for r in rows if r['pair'] == pair}
            before, after = pair_rows[labels[0]][metric], pair_rows[labels[1]][metric]
            changes.append((after / before - 1) * 100)
        rng = random.Random(seed)
        boot = [statistics.median(rng.choices(changes, k=len(changes))) for _ in range(4000)]
        result[metric] = dict(variants=stats, paired_change_percent=changes,
                             median_change_percent=statistics.median(changes),
                             bootstrap95_percent=[percentile(boot, 0.025), percentile(boot, 0.975)])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dexkit-root', type=Path, required=True)
    parser.add_argument('--corpus', type=Path, required=True)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--java-home', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', nargs=3, action='append', metavar=('LABEL', 'LIBRARY', 'VERIFIED_RUN'), required=True)
    parser.add_argument('--pairs', type=int, default=10)
    parser.add_argument('--threads', type=int, default=4)
    parser.add_argument('--passes', type=int, default=2)
    parser.add_argument('--profile', choices=['all', 'chains', 'batch'], default='all')
    parser.add_argument('--seed', type=int, default=20260915)
    parser.add_argument('--memory-probe', type=Path)
    args = parser.parse_args()
    if len(args.variant) != 2 or args.pairs < 2:
        raise SystemExit('Supply exactly two variants and at least two pairs.')
    labels = [v[0] for v in args.variant]
    if len(set(labels)) != 2 or any(not s.replace('_', '').replace('-', '').isalnum() for s in labels):
        raise SystemExit('Variant labels must be distinct simple names.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Choose a new empty output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    order = [pair % 2 for pair in range(args.pairs)]
    random.Random(args.seed).shuffle(order)
    rows = []
    runner = Path(__file__).with_name('run.py')
    for pair, reverse in enumerate(order):
        variants = list(reversed(args.variant)) if reverse else args.variant
        for position, (label, library, verified) in enumerate(variants):
            output = args.output / f'pair-{pair:02d}-{label}'
            cmd = [sys.executable, str(runner), '--dexkit-root', str(args.dexkit_root),
                   '--corpus', str(args.corpus), '--apk', str(args.apk),
                   '--java-home', str(args.java_home), '--output', str(output),
                   '--native-library', library, '--verified-run', verified,
                   '--mode', 'measure', '--profile', args.profile,
                   '--threads', str(args.threads), '--passes', str(args.passes)]
            if args.memory_probe:cmd += ['--memory-probe', str(args.memory_probe)]
            run = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            (args.output / f'pair-{pair:02d}-{label}-launcher.log').write_text(run.stdout)
            if run.returncode:
                print(run.stdout, file=sys.stderr)
                raise SystemExit(f'Failed measurement: pair {pair}, {label}')
            report = json.loads((output / 'report.json').read_text())
            metadata = json.loads((output / 'run-metadata.json').read_text())
            row = dict(pair=pair, position=position, label=label, native_sha256=metadata['native_sha256'],
                       lifecycle_ms=report['create_to_close_observed_ns'] / 1e6,
                       create_ms=report['create_ns'] / 1e6, close_ms=report['close_ns'] / 1e6,
                       max_rss_bytes=metadata.get('process_max_rss_bytes'),
                       window_peak_rss_bytes=report.get('window_peak_rss_bytes'),
                       peak_footprint_bytes=metadata.get('process_peak_footprint_bytes'),
                       window_peak_footprint_bytes=report.get('window_peak_footprint_bytes'))
            for number in range(args.passes):
                row[f'pass{number}_api_ms'] = sum(s['api_ns'] for s in report['stages'] if s['pass'] == number) / 1e6
            row['repeated_api_ms'] = sum(s['api_ns'] for s in report['stages'] if s['pass'] > 0) / 1e6
            rows.append(row)
            (args.output / 'samples.json').write_text(json.dumps(rows, indent=2) + '\n')
            print(f'pair={pair:02d} {label} lifecycle={row["lifecycle_ms"]:.2f} ms close={row["close_ms"]:.2f} ms', flush=True)
    result = dict(profile=args.profile, threads=args.threads, passes=args.passes,
                  pairs=args.pairs, seed=args.seed, labels=labels, reversed_order=order,
                  notes='Fresh JVM per sample, balanced randomized AB/BA pairs; OS file cache is not flushed. Negative change favors the second label. Bootstrap intervals are exploratory paired-median intervals, not a universal performance guarantee.',
                  metrics=summarize(rows, labels, args.pairs, args.seed))
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    for metric, value in result['metrics'].items():
        print(metric, f'{value["median_change_percent"]:+.2f}%', value['bootstrap95_percent'])


if __name__ == '__main__':
    main()
