import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
OUT = Path('/Users/teble/project/android/DexKit-benchmark-data/qq-9.3.55/narrow-types-v1/r3/validation/gradle')
OUT.mkdir()
env = os.environ.copy()
env['JAVA_HOME'] = '/Users/teble/Library/Java/JavaVirtualMachines/jbr-17.0.6/Contents/Home'
env['ANDROID_HOME'] = env['ANDROID_SDK_ROOT'] = '/Users/teble/Library/Android/sdk'
properties = ['LazyDirectories', 'CompactStrings', 'NegativeStrings', 'StructuralDescriptors',
              'DescriptorFastHits', 'RawInterfaces', 'FieldIdentitySplit', 'CompactInvokes',
              'SingleRelation', 'InvertedStrings', 'InvertedStringRanges',
              'SkipEmptyCandidates', 'MoveFindResults', 'CompactCallers', 'UncachedDescriptors', 'RawDescriptorLookup', 'NarrowTypes']
command = ['bash', './gradlew', ':dexkit:cmakeBuild', ':dexkit:jar', ':dexkit:test',
           ':dexkit-android:assembleRelease', '--max-workers=2', '-I', 'tools/qaux-benchmark/force_tests.gradle']
command += ['-Pexperiment' + name + '=ON' for name in properties]
command += ['-PexperimentCandidatePipeline=OFF', '-PexperimentCandidateSlices=OFF', '-PexperimentPackedCrossInfo=OFF', '-PexperimentPackedFieldUses=OFF', '-PexperimentHybridDescriptors=OFF', '-PexperimentVectorDescriptorsNoPromotion=OFF', '-PexperimentVectorDescriptors=OFF', '-PexperimentNodeDescriptors=OFF', '-PexperimentSparseDescriptors=OFF']
print('START JVM and Android validation', flush=True)
with (OUT/'build.log').open('w') as log:
    subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
tests = []
for source in sorted((ROOT/'dexkit/build/test-results/test').glob('TEST-*.xml')):
    shutil.copyfile(source, OUT/source.name)
    item = ET.parse(source).getroot()
    tests.append(dict(item.attrib))
assert tests and all(int(item['failures']) == 0 and int(item['errors']) == 0 for item in tests)
aars = list((ROOT/'dexkit-android/build/outputs/aar').glob('*release.aar'))
assert len(aars) == 1
with zipfile.ZipFile(aars[0]) as archive:
    abis = sorted(name.split('/')[1] for name in archive.namelist() if name.startswith('jni/') and name.endswith('/libdexkit.so'))
assert abis == ['arm64-v8a', 'armeabi-v7a', 'x86', 'x86_64']
caches = []
for abi in abis + ['desktop']:
    candidates = list((ROOT/('dexkit' if abi == 'desktop' else 'dexkit-android')).glob('**/CMakeCache.txt'))
    candidates = [p for p in candidates if (abi == 'desktop' or abi in p.parts)
                  and 'DEXKIT_EXPERIMENT_COMPACT_CALLERS:BOOL=ON' in p.read_text()]
    assert candidates, abi
    source = max(candidates, key=lambda p: p.stat().st_mtime)
    content = source.read_text()
    for name in ['LAZY_DIRECTORIES', 'COMPACT_STRINGS', 'NEGATIVE_STRINGS', 'STRUCTURAL_DESCRIPTORS',
                 'DESCRIPTOR_FAST_HITS', 'RAW_INTERFACES', 'FIELD_IDENTITY_SPLIT', 'COMPACT_INVOKES',
                 'SINGLE_RELATION', 'INVERTED_STRINGS', 'INVERTED_STRING_RANGES',
                 'SKIP_EMPTY_CANDIDATES', 'MOVE_FIND_RESULTS', 'COMPACT_CALLERS',
                 'UNCACHED_DESCRIPTORS', 'RAW_DESCRIPTOR_LOOKUP', 'NARROW_TYPES']:
        assert 'DEXKIT_EXPERIMENT_' + name + ':BOOL=ON' in content, (abi, name)
    for name in ['CANDIDATE_PIPELINE', 'CANDIDATE_SLICES', 'PACKED_CROSS_INFO', 'PACKED_FIELD_USES', 'NODE_DESCRIPTORS', 'SPARSE_DESCRIPTORS', 'HYBRID_DESCRIPTORS', 'VECTOR_DESCRIPTORS_NO_PROMOTION', 'VECTOR_DESCRIPTORS']:
        assert 'DEXKIT_EXPERIMENT_' + name + ':BOOL=OFF' in content, (abi, name)
    shutil.copyfile(source, OUT/(abi+'-CMakeCache.txt'))
    caches.append(dict(abi=abi, source=str(source), sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
shutil.copyfile(aars[0], OUT/aars[0].name)
result = dict(command=command, tests=tests, total_tests=sum(int(item['tests']) for item in tests),
              skipped=sum(int(item.get('skipped', 0)) for item in tests), abis=abis, cmake_caches=caches,
              jar_sha256=hashlib.sha256((ROOT/'dexkit/build/libs/dexkit.jar').read_bytes()).hexdigest(),
              aar_sha256=hashlib.sha256(aars[0].read_bytes()).hexdigest())
(OUT/'validation.json').write_text(json.dumps(result, indent=2)+'\n')
print('DONE tests='+str(result['total_tests'])+' skipped='+str(result['skipped'])+' abis='+','.join(abis), flush=True)
