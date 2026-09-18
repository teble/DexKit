import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E = Path(__file__).resolve().parent
OUT = E/'validation/layout'
OUT.mkdir(exist_ok=True)
source = OUT/'layout.cpp'
names = ['cache_offset', 'invoke_operand', 'method_identity', 'class_definition', 'caller_entry', 'method_reference']
types = ['CacheOffset', 'InvokeOperandId', 'LocalMethodId', 'ClassDefIndex', 'CompactCallerIndex::Entry', 'MethodReference']
source.write_text('#include "index_types.h"\n#include "compact_caller_index.h"\nextern "C" {\n' +
    '\n'.join('char '+name+'[sizeof(dexkit::'+t+')];' for name,t in zip(names,types)) + '\n}\n')
records = json.loads((OUT/'summary.json').read_text()) if (OUT/'summary.json').exists() else []
for cached in json.loads((E/'validation/gradle/validation.json').read_text())['cmake_caches']:
    abi = cached['abi']
    if any(row['abi'] == abi for row in records):
        continue
    database = Path(cached['source']).parent/'compile_commands.json'
    if not database.exists():
        build = E/'artifacts/narrow/build'
        commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands'], text=True)
        command = next(line for line in commands.splitlines() if ' -c '+str(ROOT/'Core/dexkit/dex_item.cpp') in line)
        entry = dict(directory=str(build), command=command, file=str(ROOT/'Core/dexkit/dex_item.cpp'))
        database = build/'build.ninja'
    else:
        entry = next(row for row in json.loads(database.read_text()) if row['file'].endswith('/dexkit/dex_item.cpp'))
    original = shlex.split(entry['command'])
    base = []
    skip = False
    for arg in original:
        if skip:
            skip = False
            continue
        if arg == '-o':
            skip = True
        elif arg == '-c' or arg == entry['file'] or arg.startswith(('-flto', '-DDEXKIT_EXPERIMENT_NARROW_TYPES=')):
            pass
        else:
            base.append(arg)
    variants = []
    for narrow in [0, 1]:
        assembly = OUT/(abi+'-'+str(narrow)+'.s')
        command = base+['-DDEXKIT_EXPERIMENT_NARROW_TYPES='+str(narrow), '-S', str(source), '-o', str(assembly)]
        with (OUT/(abi+'-'+str(narrow)+'.log')).open('w') as log:
            subprocess.run(command, cwd=entry['directory'], stdout=log, stderr=subprocess.STDOUT, check=True)
        text = assembly.read_text()
        sizes = {}
        for symbol in names:
            pattern = r'\.zerofill\s+__DATA,__common,_'+symbol+r',(\d+),' if abi == 'desktop' else r'\.size\s+'+symbol+r',\s*(\d+)'
            match = re.search(pattern, text)
            assert match, (abi, symbol)
            sizes[symbol] = int(match[1])
        assert sizes['method_identity'] == 4 and sizes['caller_entry'] == sizes['method_reference'] == 8
        assert sizes['invoke_operand'] == (2 if narrow else 4)
        assert sizes['class_definition'] == (2 if narrow else 4)
        assert sizes['cache_offset'] == (4 if narrow or abi in ['armeabi-v7a', 'x86'] else 8)
        variants.append(dict(narrow=bool(narrow), command=command, sizes=sizes,
                             assembly_sha256=hashlib.sha256(assembly.read_bytes()).hexdigest()))
    checks = []
    for name in ['index_types_checks', 'caller_index_checks']:
        command = base+['-DDEXKIT_EXPERIMENT_NARROW_TYPES=1', '-fsyntax-only', str(ROOT/'tools/qaux-benchmark'/(name+'.cpp'))]
        with (OUT/(abi+'-'+name+'.log')).open('w') as log:
            subprocess.run(command, cwd=entry['directory'], stdout=log, stderr=subprocess.STDOUT, check=True)
        checks.append(dict(name=name, command=command))
    records.append(dict(abi=abi, compile_database=str(database), variants=variants, component_compiles=checks))
    (OUT/'summary.json').write_text(json.dumps(records, indent=2)+'\n')
    print(abi+' '+json.dumps([v['sizes'] for v in variants]), flush=True)
