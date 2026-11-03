"""Compile/run the shared A/R transform contract on Windows x86 and x64."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args()
here=Path(__file__).resolve().parent;cpp=here/'native_viewport_projection.cpp';header=here.parents[1]/'shared/source/native_viewport_projection.h'
build=a.out.parent/'projection-bin';build.mkdir(parents=True,exist_ok=True);rows=[]
for target in ('x86-windows-gnu','x86_64-windows-gnu'):
 exe=build/(target+'.exe');flags=['-target',target,'-std=c++17','-O2','-UNDEBUG']
 subprocess.run([str(a.zig.resolve()),'c++',*flags,str(cpp),'-o',str(exe.resolve())],check=True)
 run=subprocess.run([str(exe.resolve())],check=True,capture_output=True,text=True);print(run.stdout,end='')
 match=re.search(r'checks=(\d+)',run.stdout);assert match and int(match[1])>=51
 rows.append({'target':target,'status':'PASS','checks':int(match[1]),'flags':flags,'assertions_enabled':True,'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'stdout':run.stdout})
report={'scope':__doc__,'pass':True,'results':rows,'source_sha256':{str(cpp.relative_to(here.parents[2])):hashlib.sha256(cpp.read_bytes()).hexdigest(),str(header.relative_to(here.parents[2])):hashlib.sha256(header.read_bytes()).hexdigest()},'game_process_tested':False}
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
