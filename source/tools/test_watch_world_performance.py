"""Compare retained-world watch rendering with the pre-change production body."""
import argparse, hashlib, json, subprocess
from pathlib import Path

def class_body(text, marker):
    start=text.index(marker);pos=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[pos]=='{')-(text[pos]=='}');pos+=1
    return text[start:pos+1]

def main():
    p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--baseline-dir',type=Path,required=True);a=p.parse_args()
    source=Path(__file__).resolve().parents[1];out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    old=(a.baseline_dir/'world_scene_pipeline.h').read_text(encoding='utf-8');current=(source/'external_reader/source/world_scene_pipeline.h').read_text(encoding='utf-8')
    # Expose private canvas storage in fixture-only copies of the actual class
    # bodies, to compare native Z/A as well as RGBA. No algorithm is replaced.
    bodies=[]
    for text,name in [(old,'BeforePipeline'),(current,'InspectedPipeline')]:
        body=class_body(text,'class Pipeline {').replace('class Pipeline {','class '+name+' {\npublic:',1)
        bodies.append('namespace external { namespace worldscene {\n'+body+'\n}}\n')
    old_watch=class_body((a.baseline_dir/'watch_scene.h').read_text(encoding='utf-8'),'template<class Reader,class Assets>class Renderer {')
    old_watch=old_watch.replace('class Renderer {','class BeforeRenderer {',1).replace(' Renderer(Reader&',' BeforeRenderer(Reader&',1).replace('worldscene::Pipeline','worldscene::BeforePipeline')
    bodies.append('namespace watchscene {\n'+old_watch+'\n}\n')
    (out/'watch_world_reference.h').write_text('\n'.join(bodies),encoding='utf-8')
    exe=out/'watch_world_performance.exe';cmd=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(source/'assist/tests/watch_world_performance.cpp'),str(source/'third_party/xbrz/xbrz.cpp'),'-I'+str(out),'-lgdiplus','-lgdi32','-lole32','-luser32','-ladvapi32','-o',str(exe)]
    build=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
    run=subprocess.run([str(exe)],capture_output=True,text=True);(out/'run.log').write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode()
    paths=['external_reader/source/world_scene_pipeline.h','assist/source/watch_scene.h','assist/source/watch_current_anchor.h','assist/tests/watch_world_performance.cpp','tools/test_watch_world_performance.py','external_reader/source/material_motion.h','external_reader/source/radar_scene.h']
    result={'production_sha256':{n:hashlib.sha256((source/n).read_bytes()).hexdigest() for n in paths},'baseline_sha256':{n:hashlib.sha256((a.baseline_dir/n).read_bytes()).hexdigest() for n in ['world_scene_pipeline.h','watch_scene.h']},'command':cmd,'stdout':run.stdout}
    (out/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':main()
