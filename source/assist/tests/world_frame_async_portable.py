"""Portable owner/authority tests plus existing real transport/Runtime seams."""
from pathlib import Path
import argparse,subprocess,runpy,json
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);here=Path(__file__).resolve().parent
# Reuse the current retention runner to prepare its exact controlled Win32 API.
subprocess.run(['python',str(here/'world_frame_retention.py'),'--out',str(a.out/'retention')],check=True)
for name in ['world_frame_publication','world_frame_authority']:
 exe=a.out/name;cmd=['g++','-std=c++17','-O1','-UNDEBUG','-pthread','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(a.out/'retention'),str(here/(name+'.cpp')),'-o',str(exe)];subprocess.run(cmd,check=True)
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60);(a.out/(name+'.log')).write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);assert r.returncode==0
(a.out/'result.json').write_text(json.dumps(dict(passed=True,game_tested=False))+'\n')
