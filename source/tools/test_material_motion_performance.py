"""Compare production Timeline against a pinned pre-change source header."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);a=p.parse_args()
    source=Path(__file__).resolve().parents[1];out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    old=a.baseline.read_text(encoding='utf-8-sig').replace('namespace materialmotion {','namespace materialmotion_before {',1)
    old=old.replace('#include "radar_map.h"','#include "'+(source/'external_reader/source/radar_map.h').as_posix()+'"')
    reference=out/'material_motion_reference.h';reference.write_text(old,encoding='utf-8')
    wrapper=out/'fixture.cpp';wrapper.write_text('#define RA2_REFERENCE_MOTION "'+reference.as_posix()+'"\n#include "'+(source/'assist/tests/material_motion_performance.cpp').as_posix()+'"\n',encoding='utf-8')
    exe=out/'material_motion_performance.exe';cmd=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(wrapper),'-o',str(exe)]
    results=[]
    for name,args in [('build',cmd),('test',[str(exe)])]:
        r=subprocess.run(args,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8');results.append(dict(name=name,command=args,exit_code=r.returncode,output=r.stdout+r.stderr));print(r.stdout+r.stderr,end='')
        (out/'result.json').write_text(json.dumps(dict(baseline_sha256=hashlib.sha256(a.baseline.read_bytes()).hexdigest(),source_sha256=hashlib.sha256((source/'external_reader/source/material_motion.h').read_bytes()).hexdigest(),results=results),indent=2)+'\n',encoding='utf-8');r.check_returncode()
if __name__=='__main__':main()
