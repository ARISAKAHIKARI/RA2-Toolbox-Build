"""Compile the actual GameEntryManager with controlled OS boundaries on Linux."""
import argparse,hashlib,json,subprocess,shlex,shutil,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--out',required=True,type=Path);p.add_argument('--baseline');a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
r=Path(__file__).resolve().parents[3]
shim=r'''#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#define __declspec(x)
#define __stdcall
using DWORD=uint32_t;using LONG=int32_t;using BOOL=int;using SIZE_T=size_t;using HANDLE=void*;using HMODULE=void*;using LPCWSTR=const wchar_t*;
constexpr BOOL FALSE=0;constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,SYNCHRONIZE=1;
constexpr DWORD MEM_COMMIT=0x1000,MEM_IMAGE=0x1000000,PAGE_GUARD=0x100,PAGE_NOACCESS=1,PAGE_READONLY=2,PAGE_READWRITE=4,PAGE_WRITECOPY=8,PAGE_EXECUTE=0x10,PAGE_EXECUTE_READ=0x20,PAGE_EXECUTE_READWRITE=0x40,PAGE_EXECUTE_WRITECOPY=0x80;
constexpr DWORD GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS=4,GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT=2,GET_MODULE_HANDLE_EX_FLAG_PIN=1;
struct MEMORY_BASIC_INFORMATION{void*BaseAddress;void*AllocationBase;size_t RegionSize;DWORD State,Protect,Type;};
inline unsigned fixtureReads=0,fixtureQueries=0;inline uint32_t fixtureUnreadable=0;inline bool fixtureFlush=true,fixtureProtect=true;
inline DWORD GetCurrentThreadId(){return 77;}inline HANDLE GetCurrentProcess(){return (void*)-1;}
inline HMODULE GetModuleHandleW(LPCWSTR){return (void*)0x400000;}inline BOOL GetModuleHandleExW(DWORD,LPCWSTR,HMODULE*out){*out=(void*)0x10000000;return 1;}
inline size_t VirtualQuery(const void*p,MEMORY_BASIC_INFORMATION*out,size_t){++fixtureQueries;uintptr_t at=(uintptr_t)p;uintptr_t base=at>=0x400000&&at<0xC00000?0x400000:at>=0x10000000&&at<0x10010000?0x10000000:0;if(!base)return 0;*out={(void*)base,(void*)base,base==0x400000?0x800000u:0x10000u,MEM_COMMIT,PAGE_EXECUTE_READWRITE,MEM_IMAGE};return sizeof *out;}
inline BOOL ReadProcessMemory(HANDLE,const void*p,void*out,size_t n,size_t*got){++fixtureReads;MEMORY_BASIC_INFORMATION info{};uintptr_t at=(uintptr_t)p;if(!VirtualQuery(p,&info,sizeof info)||at+n>(uintptr_t)info.BaseAddress+info.RegionSize||(fixtureUnreadable&&at<=fixtureUnreadable&&fixtureUnreadable<at+n)){*got=0;return 0;}memcpy(out,p,n);*got=n;return 1;}
inline BOOL VirtualProtect(void*,size_t,DWORD,DWORD*prior){*prior=PAGE_EXECUTE_READWRITE;return fixtureProtect;}
inline BOOL FlushInstructionCache(HANDLE,void*,size_t){return fixtureFlush;}
inline HANDLE OpenThread(DWORD,BOOL,DWORD){return (void*)1;}inline DWORD WaitForSingleObject(HANDLE,DWORD){return WAIT_TIMEOUT;}inline BOOL CloseHandle(HANDLE){return 1;}
inline LONG InterlockedCompareExchange(volatile LONG*p,LONG x,LONG cmp){LONG old=*p;if(old==cmp)*p=x;return old;}inline LONG InterlockedExchange(volatile LONG*p,LONG x){LONG old=*p;*p=x;return old;}inline LONG InterlockedIncrement(volatile LONG*p){return ++*p;}
'''
(out/'windows.h').write_text(shim)
placement=(r/'source/assist/source/placement_bridge.h').read_text()
def extract(name):
 start=placement.index('static bool '+name+'(');at=placement.index('{',start);end=at+1;depth=1
 while depth:
  if placement[end]=='{':depth+=1
  elif placement[end]=='}':depth-=1
  end+=1
 return placement[start:end]
(out/'risk_site.inc').write_text('\n'.join(extract(name) for name in ['riskSiteCurrent','restoreRiskSite','installRiskSite']))
placement=(r/'source/assist/source/control_sites_bridge.h').read_text()
(out/'control_site.inc').write_text(extract('controlSiteReady'))
files=['source/shared/source/game_entry_manager.cpp','source/shared/source/game_entry_client.h','source/assist/source/placement_bridge.h','source/assist/source/control_sites_bridge.h','source/shared/source/patch_context.h','source/assist/source/hook_registry.h','source/assist/tests/debug_patch_manager.cpp','source/assist/tests/debug_patch_manager.py']
sha=lambda f:hashlib.sha256((r/f).read_bytes()).hexdigest();before={f:sha(f) for f in files};commands=[]
for cmd in [['g++','-std=c++17','-O2','-UNDEBUG','-I',str(out),str(r/'source/assist/tests/debug_patch_manager.cpp'),'-o',str(out/'fixture')],[str(out/'fixture')]]:
 proc=subprocess.run(cmd,text=True,capture_output=True);commands.append(dict(command=cmd,exit_code=proc.returncode,stdout=proc.stdout,stderr=proc.stderr));print(proc.stdout,proc.stderr,flush=True)
 if proc.returncode:break
result=dict(passed=all(x['exit_code']==0 for x in commands),source_sha256=before,source_hash_stable=before=={f:sha(f) for f in files},game_process_executed=False,commands=commands)
match=re.search(r'PASS: (\d+) production manager',commands[-1]['stdout'])
if match:result['checks']=int(match[1])
if a.baseline and result['passed']:
 # Keep the exact current fixture and all other production headers. Replace
 # ONE entire production implementation with the committed pre-fix version,
 # so the negative control has the real old bug, not a recreated model.
 dep=subprocess.check_output(['g++','-std=c++17','-I',str(out),'-MM',str(r/'source/assist/tests/debug_patch_manager.cpp')],text=True)
 dependencies=[]
 for name in shlex.split(dep.replace('\\\n',' '))[1:]:
  path=Path(name).resolve()
  if path.is_relative_to(r):dependencies.append(path.relative_to(r))
 controls=[]
 for label,file in [('old_manager','source/shared/source/game_entry_manager.cpp'),('old_journal','source/assist/source/hook_registry.h')]:
  target=out/label/'repo';target.mkdir(parents=True,exist_ok=True)
  for rel in dependencies:
   dst=target/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(r/rel,dst)
  old=subprocess.check_output(['git','-C',str(r),'show',a.baseline+':'+file]);(target/file).write_bytes(old)
  binary=out/label/'fixture';built=subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I',str(out),str(target/'source/assist/tests/debug_patch_manager.cpp'),'-o',str(binary)],text=True,capture_output=True)
  assert built.returncode==0,built.stderr
  oldrun=subprocess.run([str(binary)],text=True,capture_output=True)
  detected=oldrun.returncode!=0 and 'manager check ' in oldrun.stderr
  controls.append(dict(name=label,baseline=a.baseline,replaced=file,baseline_sha256=hashlib.sha256(old).hexdigest(),detected=detected,exit_code=oldrun.returncode,stderr=oldrun.stderr))
  print(label,oldrun.stderr.strip(),flush=True)
 result['negative_controls']=controls;result['passed']=result['passed'] and all(x['detected'] for x in controls)
(out/'result.json').write_text(json.dumps(result,indent=2));raise SystemExit(not(result['passed'] and result['source_hash_stable']))
