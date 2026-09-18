from pathlib import Path
import hashlib
import json
import os
import subprocess

E=Path(__file__).resolve().parent
source=E/'remote_width_api_checks.cpp'
fixture=E/'fixtures/remote-wide/widths.apk'
records=[]
seen=set()
env=os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
for phase in ['normal','isolated','sanitize','combination']:
    for row in json.loads((E/'validation'/('width-api-'+phase+'.json')).read_text()):
        label=row['label']
        if label in seen:
            continue
        seen.add(label)
        command=row['compile_command'][:]
        command=[str(source) if x.endswith('/width_api_checks.cpp') else x for x in command]
        output=E/'validation'/('remote-width-api-'+label)
        command[-1]=str(output)
        with (E/'validation'/('remote-width-'+label+'-compile.log')).open('w') as log:
            subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
        result=subprocess.run([str(output),str(fixture)],env=env,capture_output=True,timeout=120)
        (E/'validation'/('remote-width-'+label+'.log')).write_bytes(result.stdout+result.stderr)
        assert result.returncode==0,(label,result.returncode,result.stderr[-2000:])
        records.append(dict(label=label,command=command,exit_code=result.returncode,
            source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),fixture_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest()))
        (E/'validation/remote-width.json').write_text(json.dumps(records,indent=2)+'\n')
        print('PASS '+label+' '+result.stdout.decode().strip(),flush=True)
assert len(records)==6
