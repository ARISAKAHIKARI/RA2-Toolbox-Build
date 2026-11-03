"""Run production radar-only clipping and composition for both Windows ABIs."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;source=here.parents[1];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
paths=['assist/source/native_viewport_host.h','assist/source/render_upgrade_host.h','shared/source/overlay_composition.h','assist/tests/radar_only_overlay.cpp','assist/tests/radar_only_overlay.py'];before={n:hashlib.sha256((source/n).read_bytes()).hexdigest()for n in paths};results={}
text=(source/'assist/source/render_upgrade_host.h').read_text(encoding='utf8');start=text.index('static bool syncRadarExclusion(');end=text.index('{',start)+1;depth=1
while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
(out/'radar_sync_extract.h').write_text(text[start:end],encoding='utf8')
for arch in ('x86','x86_64'):
 exe=out/(arch+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O2','-UNDEBUG','-I'+str(out),str(here/'radar_only_overlay.cpp'),'-o',str(exe)]
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(arch+'-build.log')).write_text(r.stdout+r.stderr,encoding='utf8');r.check_returncode()
 r=subprocess.run([str(exe)],capture_output=True,text=True);(out/(arch+'.log')).write_text(r.stdout+r.stderr,encoding='utf8');r.check_returncode();results[arch]=json.loads(r.stdout)
assert before=={n:hashlib.sha256((source/n).read_bytes()).hexdigest()for n in paths}
report={'results':results,'sources':before,'game_process_tested':False};(out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
