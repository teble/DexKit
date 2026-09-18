from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile

ROOT = Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools/qaux-benchmark'))
from make_symbol_fixture import make_dex

owner = 'Lwidth/Source;'
external = 'L0External;'
target = (external, 'target', 'V', ())
field = (owner, 'value', 'I')
needle = 'zz-jumbo-width-needle'

for label, definitions in [('limit', 65535), ('wide', 65536)]:
    out = E / 'fixtures' / label
    out.mkdir(parents=True)
    methods = {(owner, f'm{i:05d}', 'V', ()) for i in range(definitions)}
    source = (owner, f'm{definitions-1:05d}', 'V', ())
    local = (owner, 'm00000', 'V', ())
    observed = {}

    def code(methods, fields, strings):
        observed.update(method_count=len(methods), source=methods[source], local=methods[local],
                        reference=methods[target], string_id=strings[needle], field_id=fields[field])
        assert methods[target] == 0 and methods[source] == definitions
        assert strings[needle] > 65535
        choices = [methods[target], methods[local]]
        if label == 'limit':
            choices.append(methods[source])
        units = []
        for i in range(70000):
            units += [0x0071, choices[i % len(choices)], 0]
        value = strings[needle]
        return units + [0x001b, value & 65535, value >> 16, 0x0060, fields[field],
                        0x0067, fields[field], 0x000e]

    data, _ = make_dex({owner: ()}, methods, {field}, references={target},
                       additional_strings=[needle], code={source: code})
    other, _ = make_dex({external: ()}, {target}, set())
    apk = out / 'widths.apk'
    with zipfile.ZipFile(apk, 'w') as archive:
        for name, content in [('classes.dex', data), ('classes2.dex', other)]:
            entry = zipfile.ZipInfo(name, (2026, 9, 18, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(entry, content)
    observed.update(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
                    dex_sha256=hashlib.sha256(data).hexdigest(),
                    source_descriptor=owner+'->'+source[1]+'()V',
                    instructions=70004, invoke_records=70000,
                    purpose='DEX parser/index boundaries; not an Android application execution fixture')
    (out / 'manifest.json').write_text(json.dumps(observed, indent=2)+'\n')
    print(label, observed, flush=True)
