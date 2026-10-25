"""Replay private RA2Master game snapshots without distributing their contents.
The game memory and validity masks are real; only unavailable Win32 disk I/O is
stubbed. This verifies production readers, SHP decode and CPU scene composition,
not Windows presentation or pixel equality with the running game.
"""
from pathlib import Path
import argparse,ast,json,struct,subprocess,tempfile,zipfile
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('capture',type=Path);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[3]
module=ast.parse(Path(__file__).with_name('game_assets_host_test.py').read_text())
header=next(ast.literal_eval(n.value) for n in module.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='header' for t in n.targets))
header+='\nuint64_t GetTickCount64();\n'
with tempfile.TemporaryDirectory() as temp,zipfile.ZipFile(a.capture) as z:
 t=Path(temp);(t/'windows.h').write_text(header)
 exe=t/'replay';subprocess.run(['g++','-std=c++17','-O2','-I'+str(t),str(Path(__file__).with_suffix('.cpp')),'-o',str(exe)],check=True)
 session_name=next(n for n in z.namelist() if n.endswith('/session.json'));base=session_name[:-len('session.json')];session=json.loads(z.read(session_name))
 results=[]
 for stage in ['01-off','02-on','03-off']:
  regions=json.loads(z.read(base+stage+'-private-regions.json'))
  regions=[{'base':session['image_base'],'file':stage+'-game.mem','valid':stage+'-game.valid'}]+regions
  snapshot=t/'memory.bin'
  with snapshot.open('wb') as f:
   f.write(struct.pack('<I',len(regions)))
   for r in regions:
    data=z.read(base+r['file']);valid=z.read(base+r.get('valid',r['file']+'.valid'));assert len(data)==len(valid)
    f.write(struct.pack('<II',int(r['base'],16),len(data)));f.write(data);f.write(valid)
  run=subprocess.run([str(exe),str(snapshot),str(a.output/stage)],capture_output=True,text=True)
  print(stage,run.stdout.strip(),run.stderr.strip());run.check_returncode();results.append({'stage':stage,**json.loads(run.stdout)})
 (a.output/'summary.json').write_text(json.dumps({'scope':__doc__,'game_exe_sha256':session['file_sha256'],'results':results},indent=2)+'\n')
