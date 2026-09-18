from pathlib import Path
import hashlib
import json
import sys
import zipfile

ROOT=Path('/Users/teble/project/android/DexKit-qaux-benchmark')
E=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tools/qaux-benchmark'))
from make_symbol_fixture import make_dex

owner='Lwidth/LowSource;'
external='L0External;'
padding='L00Padding;'
source=(owner,'call','V',())
target=(external,'target','V',())
observed={}
def code(methods,fields,strings):
    assert methods[target]==0 and methods[source]==1
    observed.update(local_operand=methods[target],source_id=methods[source])
    return [0x0071,methods[target],0,0x000e]
local,_=make_dex({owner:()},{source},set(),references={target},code={source:code})
definitions={(padding,f'm{i:05d}','V',()) for i in range(65536)}|{target}
def target_code(methods,fields,strings):
    assert methods[target]==65536 and len(methods)==65537
    observed.update(remote_definition_id=methods[target],remote_method_count=len(methods))
    return [0x000e]
remote,_=make_dex({padding:(),external:()},definitions,set(),code={target:target_code})
out=E/'fixtures/remote-wide'
out.mkdir(parents=True)
apk=out/'widths.apk'
with zipfile.ZipFile(apk,'w') as archive:
    for name,data in [('classes.dex',local),('classes2.dex',remote)]:
        entry=zipfile.ZipInfo(name,(2026,9,18,0,0,0))
        entry.compress_type=zipfile.ZIP_DEFLATED
        archive.writestr(entry,data)
observed.update(apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),
    purpose='Low local invoke operand resolves to a remote method definition above 65535')
(out/'manifest.json').write_text(json.dumps(observed,indent=2)+'\n')
print(json.dumps(observed))
