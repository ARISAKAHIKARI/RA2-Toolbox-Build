from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
s=Path(__file__).resolve().parents[1];h=s/'assist/source/player_test_bridge.h';t=h.read_text(encoding='utf8');start=t.index('struct DestructSelectedItem');end=t.index('static bool authorizePlayerOrder',start);(a.out/'destruct_selection_extract.h').write_text(t[start:end],encoding='utf8')
r={}
for arch in ('x86','x86_64'):
 exe=a.out/(arch+'.exe');cmd=[str(a.zig.resolve()),'c++','-std=c++17','-O2','-UNDEBUG','-target',arch+'-windows-gnu',str(s/'assist/tests/destruct_selection.cpp'),'-I'+str(a.out.resolve()),'-o',str(exe.resolve())];build=subprocess.run(cmd,capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(build.stdout+build.stderr);build.check_returncode();run=subprocess.run([str(exe.resolve())],capture_output=True,text=True);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr);print(run.stdout,end='');run.check_returncode();r[arch]=run.stdout.strip()
(a.out/'result.json').write_text(json.dumps({'results':r,'header_sha256':hashlib.sha256(h.read_bytes()).hexdigest(),'test_sha256':hashlib.sha256((s/'assist/tests/destruct_selection.cpp').read_bytes()).hexdigest()},indent=2))
