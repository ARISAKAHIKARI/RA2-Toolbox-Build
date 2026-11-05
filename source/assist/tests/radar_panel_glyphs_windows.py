import argparse,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;source=here.parents[1];out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
text=(source/'shared/source/gpu_compositor.h').read_text(encoding='utf-8-sig')
parameters=re.search(r' struct Parameters \{.*?\};',text,re.S).group()
shader=re.search(r' static constexpr const char\*Shader=R"HLSL\(.*?\)HLSL";',text,re.S).group()
inc=out/'production-shader.inc';inc.write_text(parameters+'\n'+shader+'\n',encoding='utf-8')
results={}
for name in ('radar_panel_glyphs_windows','radar_output_scene'):
 exe=out/(name+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(here/(name+'.cpp')),'-DRADAR_PANEL_SHADER="'+inc.as_posix()+'"','-o',str(exe),'-lgdi32','-luser32','-ldxguid']
 cmd.append(str(source/'third_party/xbrz/xbrz.cpp'))
 built=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-build.log')).write_text(built.stdout+built.stderr,encoding='utf-8')
 if built.returncode:raise SystemExit(built.stdout+built.stderr)
 ran=subprocess.run([str(exe)],capture_output=True,text=True);(out/(name+'.log')).write_text(ran.stdout+ran.stderr,encoding='utf-8');print(ran.stdout+ran.stderr)
 results[name]={'returncode':ran.returncode,'output':ran.stdout+ran.stderr};(out/'result.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
 if ran.returncode:raise SystemExit(ran.returncode)
