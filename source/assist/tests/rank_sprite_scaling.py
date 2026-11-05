"""Source-bound rank-only sprite reconstruction and pinned negative control."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,time
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
source=Path(__file__).resolve().parents[2];out=a.out.resolve();out.mkdir(parents=True,exist_ok=True);fixture=Path(__file__).resolve().parent/'fixtures/rank_scaling_baseline';manifest=json.loads((fixture/'manifest.json').read_text(encoding='utf-8'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for n,r in manifest['files'].items():assert sha(fixture/n)==r['sha256']
def body(text,marker):
 start=text.index(marker);at=text.index('{',start)+1;depth=1
 while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
 return text[start:at+1]
old=(fixture/'local_ui_renderer.h').read_text(encoding='utf-8');new=(source/'external_reader/source/local_ui_renderer.h').read_text(encoding='utf-8')
before=body(old,'class Painter {').replace('Painter','BeforePainter')+'\n'+body(old,'class Engine {').replace('class Engine {','class BeforeEngine {\npublic:',1).replace('Painter','BeforePainter')
current=body(new,'class Engine {').replace('class Engine {','class InspectedEngine {\npublic:',1)
classes=out/'production-classes.inc';classes.write_text('namespace external {namespace localui {\n'+before+'\n'+current+'\n}}\n',encoding='utf-8')
panel=out/'baseline-panel.inc';panel.write_text((fixture/'panel_glyphs.h').read_text(encoding='utf-8').replace('namespace panelglyphs {','namespace before_panelglyphs {'),encoding='utf-8')
old_cpu=(fixture/'xbrz_adapter.h').read_text(encoding='utf-8');old_cpu=old_cpu[old_cpu.index('struct ScaledImage {'):old_cpu.index('// Complete composited terrain')]
cpu=out/'baseline-sprite.inc';cpu.write_text('namespace before_spritecpu {constexpr size_t MaxPixels=16*1024*1024;\n'+old_cpu+'}\n',encoding='utf-8')
paths=['shared/source/screen_sprite_scale.h','shared/source/sprite_scale_cpu.h','shared/source/panel_glyphs.h','external_reader/source/xbrz_adapter.h','external_reader/source/local_ui_renderer.h','external_reader/source/local_ui_task.h','assist/tests/rank_sprite_scaling.cpp','assist/tests/rank_sprite_scaling.py'];before_sha={n:sha(source/n) for n in paths}
exe=out/'rank_sprite_scaling.exe';command=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(source/'assist/tests/rank_sprite_scaling.cpp'),str(source/'third_party/xbrz/xbrz.cpp'),'-DRANK_BASELINE_CLASSES="'+classes.as_posix()+'"','-DRANK_BASELINE_PANEL="'+panel.as_posix()+'"','-DRANK_BASELINE_CPU="'+cpu.as_posix()+'"','-o',str(exe),'-lgdi32','-luser32','-lgdiplus','-lole32']
started=time.perf_counter();build=subprocess.run(command,capture_output=True,text=True,encoding='utf-8');build_seconds=time.perf_counter()-started;(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8');print(build.stdout+build.stderr,end='');build.check_returncode()
started=time.perf_counter();run=subprocess.run([str(exe)],capture_output=True,text=True,encoding='utf-8');run_seconds=time.perf_counter()-started;(out/'run.log').write_text(run.stdout+run.stderr,encoding='utf-8');print(run.stdout+run.stderr,end='');run.check_returncode();assert before_sha=={n:sha(source/n) for n in paths}
result={'passed':True,'scope':__doc__,'source_hash_stable':True,'source_sha256':before_sha,'baseline':manifest,'stdout':run.stdout,'command':command,'build_seconds':build_seconds,'run_seconds':run_seconds,'game_tested':False};(out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
