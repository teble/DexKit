#!/usr/bin/env python3
"""Balanced fresh-process timings for bounded native-only counterexamples."""

import argparse
import hashlib
import json
from pathlib import Path
import random
import re
import subprocess

from sweep import summarize


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--variant', nargs=2, action='append', metavar=('LABEL', 'ARTIFACT'), required=True)
    parser.add_argument('--mode', choices=['output', 'output-sso', 'lookup', 'lookup-prefix', 'lookup-hot', 'interfaces',
                                         'field-forward', 'field-late', 'field-full-first',
                                         'invoke-output', 'caller-output', 'invoke-match', 'caller-match',
                                         'invoke-early', 'caller-early', 'invoke-multiple', 'caller-multiple',
                                         'source-output', 'source-match', 'source-hot',
                                         'string-eq', 'string-eq-long', 'string-prefix', 'string-prefix-long',
                                         'string-prefix-tail', 'string-prefix-multiple', 'string-prefix-class', 'string-prefix-sparse',
                                         'string-class', 'string-sparse', 'string-contains', 'string-multiple',
                                         'batch-method', 'batch-class'], required=True)
    parser.add_argument('--repeats', type=int, required=True)
    parser.add_argument('--pairs', type=int, default=6)
    parser.add_argument('--seed', type=int, default=2026091511)
    args = parser.parse_args()
    labels = [v[0] for v in args.variant]
    if len(labels) != 2 or len(set(labels)) != 2 or args.pairs < 2 or not 2 <= args.repeats <= 100000:
        raise SystemExit('Use two distinct labels, at least two pairs and 2..100000 repetitions.')
    if any(not label.replace('-', '').replace('_', '').isalnum() for label in labels):
        raise SystemExit('Use simple variant labels.')
    if args.output.exists() and any(args.output.iterdir()):
        raise SystemExit('Use a new empty output directory.')
    args.output.mkdir(parents=True, exist_ok=True)
    fixture_hash = sha(args.fixture)
    variants = []
    for label, directory in args.variant:
        artifact = Path(directory).resolve()
        manifest = json.loads((artifact / 'artifact.json').read_text())
        invocation = args.mode.startswith(('invoke-', 'caller-'))
        source = args.mode.startswith('source-')
        string = args.mode.startswith('string-')
        batch = args.mode.startswith('batch-')
        relation = args.mode.startswith('field-') or invocation
        executable = artifact / ('build/Core/dexkit_batch_workload' if batch else
                                 'build/Core/dexkit_string_workload' if string else
                                 'build/Core/dexkit_source_workload' if source else
                                 'build/Core/dexkit_invocation_workload' if invocation else
                                 'build/Core/dexkit_relation_workload' if relation else 'build/Core/dexkit_descriptor_workload')
        if manifest['diagnostics'] or manifest['native_sha256'] != sha(artifact / 'libdexkit.dylib'):
            raise SystemExit('Require unchanged, non-diagnostic native artifacts.')
        if any(manifest['cmake_options'].get(key) != 'OFF' for key in
               ['DEXKIT_ENABLE_INTERNAL_METRICS', 'DEXKIT_ENABLE_INTERNAL_METRICS_API']):
            raise SystemExit('Timed workload artifacts must disable both internal metrics options.')
        option = ('DEXKIT_BENCHMARK_BATCH_WORKLOAD' if batch else
                  'DEXKIT_BENCHMARK_STRING_WORKLOAD' if string else
                  'DEXKIT_BENCHMARK_SOURCE_WORKLOAD' if source else 'DEXKIT_BENCHMARK_RELATION_WORKLOAD'
                  if relation else 'DEXKIT_BENCHMARK_DESCRIPTOR_WORKLOAD')
        if manifest['cmake_options'][option] != 'ON':
            raise SystemExit('Artifact must include the workload executable.')
        variants.append(dict(label=label, executable=str(executable), executable_sha256=sha(executable), artifact=manifest))
    order = [pair % 2 for pair in range(args.pairs)]
    random.Random(args.seed).shuffle(order)
    rows, expected = [], None
    for pair, reverse in enumerate(order):
        for position, variant in enumerate(reversed(variants) if reverse else variants):
            executable = Path(variant['executable'])
            if sha(executable) != variant['executable_sha256'] or sha(args.fixture) != fixture_hash:
                raise SystemExit('Input changed during the sweep.')
            command = ['/usr/bin/time', '-l', str(executable), str(args.fixture.resolve()), args.mode, str(args.repeats)]
            run = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=300)
            (args.output / f'pair-{pair:02d}-{variant["label"]}.log').write_text(run.stdout)
            if run.returncode:
                raise SystemExit(run.stdout)
            reports = re.findall(r'^WORKLOAD (.+)$', run.stdout, re.M)
            if len(reports) != 1:
                raise SystemExit('Require exactly one workload report.')
            report = json.loads(reports[0])
            identity = (report['mode'], report['repeats'], report['checksum'], report['returned'])
            if expected is None:
                expected = identity
            if identity != expected or report['mode'] != args.mode or report['repeats'] != args.repeats:
                raise SystemExit('Workload result identity differs.')
            row = dict(pair=pair, position=position, label=variant['label'], report=report,
                       lifecycle_ms=report['lifecycle_ns'] / 1e6, create_ms=report['create_ns'] / 1e6,
                       close_ms=report['close_ns'] / 1e6, setup_ms=report['setup_ns'] / 1e6,
                       positive_ms=report['positive_ns'] / 1e6, negative_ms=report['negative_ns'] / 1e6,
                       pass0_api_ms=report['first_ns'] / 1e6, repeated_api_ms=report['repeated_ns'] / 1e6)
            for part in ['forward_first', 'forward_repeated', 'reverse_first', 'reverse_repeated']:
                if part + '_ns' in report: row[part + '_ms'] = report[part + '_ns'] / 1e6
            for metric, title in [('max_rss_bytes', 'maximum resident set size'), ('peak_footprint_bytes', 'peak memory footprint')]:
                match = re.search(r'^\s*(\d+)\s+' + title + r'\s*$', run.stdout, re.M)
                if not match:
                    raise SystemExit('Missing process memory measurement.')
                row[metric] = int(match[1])
            rows.append(row)
            (args.output / 'samples.json').write_text(json.dumps(rows, indent=2) + '\n')
            print(f'pair={pair:02d} {variant["label"]} lifecycle={row["lifecycle_ms"]:.3f} ms', flush=True)
    result = dict(mode=args.mode, repeats=args.repeats, pairs=args.pairs, seed=args.seed,
                  fixture_sha256=fixture_hash, variants=variants, reversed_order=order,
                  notes='Native-only fixture; includes setup, output serialization/destruction and bridge close. '
                        'Balanced fresh processes; file caches are not flushed. Negative changes favor the second label. '
                        'Paired bootstrap intervals are exploratory, not a proof of zero regression.',
                  metrics=summarize(rows, labels, args.pairs, args.seed,
                      metrics=['lifecycle_ms','create_ms','setup_ms','close_ms','pass0_api_ms',
                               'repeated_api_ms','positive_ms','negative_ms','max_rss_bytes','peak_footprint_bytes',
                               'forward_first_ms','forward_repeated_ms','reverse_first_ms','reverse_repeated_ms']))
    (args.output / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
    for metric, value in result['metrics'].items():
        print(metric, f'{value["median_change_percent"]:+.2f}%', value['bootstrap95_percent'])


if __name__ == '__main__':
    main()
