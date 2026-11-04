"""Actual watch lifecycle/Host entry and Sampler batching regressions."""
import argparse, hashlib, json, subprocess
from pathlib import Path

def function(text, marker):
    start=text.index(marker); pos=text.index('{',start)+1; depth=1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}'); pos+=1
    return text[start:pos]

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);a=p.parse_args()
    source=Path(__file__).resolve().parents[1];out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    (out/'watch_lifetime_entry_extract.h').write_text(function((source/'assist/source/watch_host.h').read_text(encoding='utf-8'),'static bool synchronizeWorld(')+'\n',encoding='utf-8')
    for name in ['watch_lifecycle','watch_reader_batch','watch_observation','watch_renderer_idle_windows']:
        exe=out/(name+'.exe');cmd=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(source/f'assist/tests/{name}.cpp'),str(source/'third_party/xbrz/xbrz.cpp'),'-I'+str(out),'-lgdiplus','-lgdi32','-lole32','-luser32','-ladvapi32','-o',str(exe)]
        build=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-build.log')).write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
        run=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode()
    paths=['assist/source/watch_host.h','assist/source/watch_scene.h','assist/source/watch_world_lifetime.h','assist/source/watch_read_view.h','assist/source/watch_observation.h','external_reader/source/watch_focus.h']
    (out/'sources.json').write_text(json.dumps({n:hashlib.sha256((source/n).read_bytes()).hexdigest() for n in paths},indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
