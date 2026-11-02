"""Compile the real native Place helper with current production enqueue code."""
import argparse, hashlib, json, subprocess
from pathlib import Path

def function(text, marker):
    start=text.index(marker); brace=text.index('{',start); depth=1; pos=brace+1
    while depth:
        depth += (text[pos]=='{')-(text[pos]=='}'); pos+=1
    return text[start:pos]

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);a=p.parse_args()
    source=Path(__file__).resolve().parents[1];out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    bridge=(source/'assist/source/bridge.cpp').read_text(encoding='utf-8')
    extracted='\n'.join(function(bridge,m) for m in ('static bool enqueue(', 'static bool submitConstructed('))
    (out/'native_placement_entry_extract.h').write_text(extracted+'\n',encoding='utf-8')
    exe=out/'native-placement.exe'
    cmd=[str(a.zig.resolve()),'c++','-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-Wl,--stack,1048576',str(source/'assist/tests/native_placement_bridge.cpp'),'-I'+str(out),'-o',str(exe),'-luser32']
    build=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
    run=subprocess.run([str(exe)],capture_output=True,text=True);(out/'test.log').write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode()
    names=['assist/source/bridge.cpp','assist/source/native_placement_bridge.h','assist/source/native_placement_preview_bridge.h','assist/source/native_placement_capture.h','assist/source/native_placement_policy.h','assist/source/native_placement_geometry.h','assist/source/native_placement_completion_bridge.h','external_reader/source/projection.h','assist/source/constructor_adapter.h','assist/source/vanilla_constructor_catalog.h','assist/tests/native_placement_bridge.cpp']
    (out/'sources.json').write_text(json.dumps({n:hashlib.sha256((source/n).read_bytes()).hexdigest() for n in names},indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
