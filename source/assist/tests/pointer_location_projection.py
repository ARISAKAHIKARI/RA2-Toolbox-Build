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
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--zig',type=Path)
    p.add_argument('--portable',action='store_true',help='Run the host compiler with explicit minimal Win32 type substitutes; no Windows execution is claimed.')
    p.add_argument('--cxx',default='g++')
    p.add_argument('--pointer-source',type=Path,default=SRC/'pointer_bridge.h')
    p.add_argument('--out',type=Path,required=True)
    a=p.parse_args()
    if not a.portable and not a.zig:p.error('--zig is required unless --portable is selected')
    a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    production=a.pointer_source.read_text(encoding='utf-8');(a.out/'pointer_location_extract.h').write_text(function(production,'static bool pointerLocation(')+'\n',encoding='utf-8')
    paths=[a.pointer_source,SRC/'native_viewport_query.h',SRC/'protocol.h',SRC.parents[1]/'shared/source/native_viewport_projection.h',HERE/'pointer_location_projection.cpp',Path(__file__).resolve()]
    hashes={str(path.resolve()):hashlib.sha256(path.read_bytes()).hexdigest()for path in paths}
    report={'scope':__doc__,'architectures':{},'portable':a.portable,'game_process_tested':False,'windows_hooks_tested':False,'multiplayer_tested':False,'native_dependency_boundary':'synthetic; real entry policy covered by native_entry_runtime.py'}
    if a.portable:
        stub=a.out/'portable-include';stub.mkdir(exist_ok=True)
        (stub/'windows.h').write_text('#pragma once\n#include <stdint.h>\n#include <atomic>\n#define __cdecl\nstruct POINT {int32_t x,y;};\ninline void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);}\n',encoding='utf-8')
        report['win32_substitutes_sha256']=hashlib.sha256((stub/'windows.h').read_bytes()).hexdigest()
    for arch in (('portable-host',)if a.portable else('x86','x86_64')):
        exe=a.out/(arch+(''if a.portable else'.exe'))
        compiler=[a.cxx,'-I'+str(stub)]if a.portable else[str(a.zig.resolve()),'c++','-target',arch+'-windows-gnu']
        cmd=[*compiler,'-std=c++17','-O2','-UNDEBUG','-I'+str(a.out),str(HERE/'pointer_location_projection.cpp'),'-o',str(exe)]
        build=subprocess.run(cmd,capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
        run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode();report['architectures'][arch]=json.loads(run.stdout)
    assert hashes=={str(path.resolve()):hashlib.sha256(path.read_bytes()).hexdigest()for path in paths}
    report['source_sha256']=hashes;(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
