"""Compile extracted production pointerLocation with actual A/R and query guards.

Native screen-to-world and ground-height queries use explicit mutation seams.
No live game, event queue or real desktop operation is performed.
"""
from pathlib import Path
import argparse,hashlib,json,subprocess
HERE=Path(__file__).resolve().parent;SRC=HERE.parent/'source'

def function(text,needle):
    start=text.index(needle);brace=text.index('{',start);at=brace+1;depth=1
    while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    production=(SRC/'pointer_bridge.h').read_text(encoding='utf-8');(a.out/'pointer_location_extract.h').write_text(function(production,'static bool pointerLocation(')+'\n',encoding='utf-8')
    report={'scope':__doc__,'architectures':{}}
    for arch in ('x86','x86_64'):
        exe=a.out/(arch+'.exe');cmd=[str(a.zig.resolve()),'c++','-std=c++17','-O2','-UNDEBUG','-target',arch+'-windows-gnu','-I'+str(a.out.resolve()),str(HERE/'pointer_location_projection.cpp'),'-o',str(exe)]
        build=subprocess.run(cmd,capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode();report['architectures'][arch]=json.loads(run.stdout)
    paths=[SRC/'pointer_bridge.h',SRC/'native_viewport_query.h',SRC/'protocol.h',SRC.parents[1]/'shared/source/native_viewport_projection.h',HERE/'pointer_location_projection.cpp',Path(__file__).resolve()]
    report['source_sha256']={str(p.relative_to(HERE.parents[1])).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest()for p in paths};(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
