"""Run production zoom level/MAX math for both Windows ABIs, no game process."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;source=here.parents[1];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
paths=['assist/source/view_zoom.h','assist/tests/view_zoom_modes.cpp','assist/tests/view_zoom_modes.py'];before={n:hashlib.sha256((source/n).read_bytes()).hexdigest()for n in paths};results={}
for arch in ('x86','x86_64'):
 exe=out/(arch+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(here/'view_zoom_modes.cpp'),'-o',str(exe)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(arch+'-build.log')).write_text(r.stdout+r.stderr,encoding='utf8');r.check_returncode()
 r=subprocess.run([str(exe)],capture_output=True,text=True);(out/(arch+'.log')).write_text(r.stdout+r.stderr,encoding='utf8');r.check_returncode();results[arch]=json.loads(r.stdout)
assert before=={n:hashlib.sha256((source/n).read_bytes()).hexdigest()for n in paths}
report={'results':results,'sources':before,'game_process_tested':False};(out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
