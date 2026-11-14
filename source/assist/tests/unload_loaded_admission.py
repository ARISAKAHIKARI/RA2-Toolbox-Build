"""Execute production unload admission with real own/live rules and pre.7 control."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[3];bridge=root/'source/assist/source/bridge.cpp';source=root/'source/assist/source/native_orders_bridge.h'
def block(text,marker):
 start=text.index(marker);at=text.index('{',start);depth=1;end=at+1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
old=subprocess.check_output(['git','show','c46aa4be4c0b562ca163a6df6f510459a4872ca6:source/assist/source/native_orders_bridge.h'],cwd=root).decode();rows=[]
for name,text,expected in [('current',source.read_text(),0),('pre7',old,1)]:
 code='\n'.join(block(bridge.read_text(),x) for x in ['static bool live(','static int kind(','static bool own('])+'\n'
 if 'static bool unloadPassengerEligible(' in text:code+=block(text,'static bool unloadPassengerEligible(')+'\n'
 code+=text[text.index('struct UnloadRosterEntry {'):text.index('static bool unloadCarrierEligible(')]
 code+=block(text,'static int smartUnloadNow(')+'\n'
 header=a.out/(name+'.h');header.write_text(code);exe=a.out/name
 r=subprocess.run(['g++','-std=c++17','-O1','-UNDEBUG','-DUNLOAD_ADMISSION_HEADER="'+str(header)+'"',str(Path(__file__).with_suffix('.cpp')),'-o',str(exe)],capture_output=True,text=True);(a.out/(name+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
 r=subprocess.run([str(exe)],capture_output=True,text=True);(a.out/(name+'-run.log')).write_text(r.stdout+r.stderr);print(name,r.returncode,r.stdout+r.stderr);rows.append(dict(name=name,exit=r.returncode,expected=expected,passed=r.returncode==expected and (name!='pre7' or 'FAIL loaded passenger lost before Deploy' in r.stderr),extracted_sha256=hashlib.sha256(header.read_bytes()).hexdigest()))
(a.out/'result.json').write_text(json.dumps(dict(passed=all(x['passed'] for x in rows),cases=rows,real_game=False),indent=2)+'\n');assert all(x['passed'] for x in rows),rows
