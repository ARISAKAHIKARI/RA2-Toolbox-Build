#!/usr/bin/env python3
"""Build an isolated Hook test host and opt-in-camera Runtime from an existing x64 base build.

First build this checkout with build-zig-x64.sh. Reuse only its unchanged x86
runtime/entry/SEH/cursor objects and broker, then recompile the Assist bridge
with RA2_HOOK_DIAGNOSTICS. No production build flags are changed by this tool.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--base-build',required=True,type=Path);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 source=Path(__file__).resolve().parent.parent;out=a.output.resolve();base=a.base_build.resolve();zig=str(Path(a.zig).resolve())
 if out==base or out==source:raise ValueError('Use a separate diagnostic output directory')
 if subprocess.check_output([zig,'version'],text=True).strip()!='0.13.0':raise ValueError('Zig 0.13.0 required')
 obj=out/'obj';obj.mkdir(parents=True,exist_ok=True);runtime=base/'obj'/'runtime-x86'
 commands=[]
 def run(label,args):
  cmd=[zig,*map(str,args)];print('BUILD '+label,flush=True)
  with (out/(label+'.log')).open('wb') as log:r=subprocess.run(cmd,cwd=source,stdout=log,stderr=subprocess.STDOUT)
  commands.append(dict(name=label,command=cmd,exit_code=r.returncode))
  (out/'build-commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
  if r.returncode:print((out/(label+'.log')).read_text(errors='replace')[-12000:]);raise SystemExit(r.returncode)
  print('PASS '+label,flush=True)
 run('diagnostic-bridge',['c++','-target','x86-windows-gnu','-std=c++17','-O2','-fms-extensions','-fasm-blocks','-fno-stack-protector','-fno-builtin','-fno-exceptions','-fno-rtti','-DRA2_HOOK_DIAGNOSTICS','-c','assist/source/bridge.cpp','-o',obj/'bridge.o'])
 run('diagnostic-runtime',['c++','-target','x86-windows-gnu','-shared',runtime/'runtime.o',runtime/'game_entry_manager.o',obj/'bridge.o',runtime/'guard.o',runtime/'hide.o',runtime/'resource_guard.o','game_runtime/source/runtime.def',runtime/'dll_version.res','-o',out/'GameRuntime.dll','-lkernel32','-luser32','-lgdi32','-lwinmm'])
 shutil.copy2(base/'GameRuntimeHost32.exe',out/'GameRuntimeHost32.exe')
 def sha(name):return hashlib.sha256((out/name).read_bytes()).hexdigest()
 (obj/'hook_test_bundle.h').write_text('#pragma once\ninline constexpr const wchar_t*HookTestRuntimeSha=L"'+sha('GameRuntime.dll')+'";\ninline constexpr const wchar_t*HookTestBrokerSha=L"'+sha('GameRuntimeHost32.exe')+'";\n',encoding='utf-8')
 run('host-resource',['rc','/fo',obj/'app.res','hook_diagnostics/source/app.rc'])
 run('diagnostic-host',['c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-municode','-I'+str(obj),'hook_diagnostics/source/main.cpp',obj/'app.res','-o',out/'RA2HookTest.exe','-Wl,--subsystem,windows','-lkernel32','-luser32','-lwinmm','-lgdi32','-ladvapi32'])
 if os.name=='nt':
  subprocess.run([str(out/'RA2HookTest.exe'),'--ui-smoke'],cwd=out,check=True,timeout=20)
  print('PASS Windows hidden UI, selection/exclusion controls, baseline opt-in, stopped-OOS skip, bundle identity and invalid PID refusal; no game attached',flush=True)
  subprocess.run([str(out/'RA2HookTest.exe'),'--broker-smoke'],cwd=out,check=True,timeout=20)
  print('PASS actual x64 host/x86 broker handshake and Runtime DLL loader; no game hooks installed',flush=True)
 (out/'BUILD_TARGET.json').write_text(json.dumps(dict(host='x64',runtime='x86',broker='x86',assist_protocol=96,cursor_protocol=155,runtime_protocol=4,diagnostic_camera_protocol=1,game_tested=False,multiplayer_tested=False,scope='isolated groups; baselines optional/off; manual site exclusions; early verdict; focus retains hooks and pauses observation; automatic camera explicit test-only opt-in'),indent=2),encoding='utf-8')
 if (base/'BUILD_SOURCE_MANIFEST.json').exists():shutil.copy2(base/'BUILD_SOURCE_MANIFEST.json',out/'BUILD_SOURCE_MANIFEST.json')
 print('PASS diagnostic bundle built',flush=True)
if __name__=='__main__':main()
