#!/usr/bin/env python3
"""Compile actual Cursor startup/start/status functions with isolated Win32 stubs."""
from pathlib import Path
import argparse,json,subprocess

def function(text,name):
 start=text.index(name);left=text.index('{',start);depth=1;end=left+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
 source=Path(__file__).resolve().parents[2];text=(source/'cursor/source/main.cpp').read_text(encoding='utf-8')
 startup=text[text.index(' const bool blankPreset=',text.index('int WINAPI wWinMain')):text.index(' HANDLE mutex=',text.index('int WINAPI wWinMain'))]
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
 unit=r'''
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdio>
#include <stdexcept>
static std::wstring saved,previousDir,root,mode=L"keep",gameDir=L"fixture",lastStatus;
static bool transparent=false,noCursorMode=false,nativeGameRefresh=false,autoStart=true,autoPaused=false,manualCursorPause=false,running=false;
static bool playbackReady=true,loaded=true,readRestricted=true,hideConfirmed=false;
static DWORD nativeStartAt,lastNativePublishAt,nativeInvisibleAt;static int lastFrame;static size_t lastPhase;
static HWND primaryAction=(HWND)1,subTitleLabel=(HWND)2,statusw=(HWND)3,mainw=(HWND)4,game=(HWND)5;
struct State {LONG state=0,command=0,tooltipState=0;};static State state,*shared=&state;
static std::map<HWND,std::wstring> labels;
static unsigned hides,resources,restores,profileIntReads;
static std::wstring ini(){return L"isolated.ini";}
static bool exists(const std::wstring&){return transparent;}
static DWORD TestModule(HMODULE,wchar_t*out,DWORD n){wcsncpy(out,L"C:\\fixture\\cursor.exe",n);return 21;}
static DWORD TestProfile(const wchar_t*section,const wchar_t*key,const wchar_t*def,wchar_t*out,DWORD n,const wchar_t*){
 const std::wstring value=!wcscmp(section,L"Startup")&&!wcscmp(key,L"Mode")?saved:!wcscmp(section,L"Target")?previousDir:def;
 wcsncpy(out,value.c_str(),n);out[n-1]=0;return DWORD(wcslen(out));
}
static UINT TestProfileInt(const wchar_t*,const wchar_t*,int,const wchar_t*){++profileIntReads;return 0;}
static bool TestWindow(HWND w){return w!=nullptr;}
static void textIfChanged(HWND w,const std::wstring&s){labels[w]=s;}
static BOOL TestInvalidate(HWND,const RECT*,BOOL){return TRUE;}
#define GetModuleFileNameW TestModule
#define GetPrivateProfileStringW TestProfile
#define GetPrivateProfileIntW TestProfileInt
#define IsWindow TestWindow
#define InvalidateRect TestInvalidate
static bool live(){return true;}static void attach(){}static void mouse(){}static void detectAres(){}
static bool manuallyBlankSHA(){return false;}
static void fail(const char*s){throw std::runtime_error(s);}
static std::wstring widen(const char*s){return std::wstring(s,s+strlen(s));}
static void log(const std::wstring&){}static void status(const std::wstring&s){lastStatus=s;}
static void hookStart(bool hide=true){hide?++hides:++resources;}
static void hookStop(){++restores;}
struct Reset {void cancel(){}void reset(){}void resetObservation(){}};static Reset burst,tween,timing;
static std::shared_ptr<int> activeMotion;
struct Asset {std::vector<int>frames=std::vector<int>(517);};static Asset asset;
'''
 unit+='\nstatic void startup(wchar_t*cmd){'+startup+'}\n'+function(text,'static void updatePrimaryAction()')+'\n'+function(text,'static void start()')
 unit+=r'''
int main(){
 unsigned checked=0;
 auto select=[&](const wchar_t*remembered,const wchar_t*command,const wchar_t*expected,bool priorTransparent=false){
  saved=remembered;previousDir=priorTransparent?L"C:\\fixture":L"";transparent=priorTransparent;mode=L"keep";noCursorMode=false;profileIntReads=0;
  std::wstring cmd=command;startup(cmd.data());assert(mode==expected&&!profileIntReads);++checked;
 };
 for(unsigned repeat=0;repeat<2;++repeat)for(const wchar_t*savedMode:{L"hide",L"keep",L"sha"})select(savedMode,L"",savedMode);
 select(L"",L"",L"keep");select(L"invalid",L"",L"keep");select(L"",L"",L"sha",true);
 select(L"keep",L"--hide",L"hide");select(L"hide",L"--keep",L"keep");select(L"hide",L"--sha",L"sha");
 select(L"keep",L"--lanbo",L"hide");select(L"keep",L"--no-cursor",L"hide");assert(noCursorMode);
 for(bool native:{false,true})for(const wchar_t*selected:{L"hide",L"keep",L"sha"}){
  mode=selected;transparent=mode==L"sha";nativeGameRefresh=native;running=false;hides=resources=restores=0;
  start();assert(running&&hides==(mode==L"hide"?1u:0u)&&!resources&&!restores);++checked;
 }
 mode=L"keep";transparent=false;asset.frames.resize(450);running=false;hides=resources=0;start();assert(!hides&&resources==1);++checked;
 mode=L"hide";running=false;hides=resources=0;start();assert(hides==1&&!resources);++checked;
 auto stateLabel=[&](LONG hide,LONG command,LONG tooltip,const wchar_t*expected){state={hide,command,tooltip};updatePrimaryAction();assert(labels[statusw]==expected);++checked;};
 stateLabel(0,1,0,L"等待游戏执行隐藏原光标");stateLabel(2,2,0,L"原光标恢复待完成");
 stateLabel(-1,0,0,L"隐藏原光标未成功 · 展开查看原因");stateLabel(1,0,0,L"原光标已隐藏 · 等待提示修复");
 stateLabel(1,0,-1,L"原光标已隐藏 · 名称和价格提示修复未成功");stateLabel(1,0,1,L"运行中 · 原光标已隐藏");
 assert(labels[primaryAction]==L"暂停"&&labels[subTitleLabel]==L"隐藏原光标 · 保留名称和价格提示");
 printf("PASS %u actual Cursor startup/start/status checks: saved preferences, command overrides, cold Hide independent of frame/native display, resource-only Keep, truthful pending/failure/ready UI\n",checked);
}
'''
 cpp=out/'cursor_startup_activation.cpp';cpp.write_text(unit,encoding='utf-8')
 results=[]
 for target in ['x86-windows-gnu','x86_64-windows-gnu']:
  exe=out/f'cursor_startup_activation-{target}.exe';cmd=[str(Path(a.zig).resolve()),'c++','-target',target,'-std=c++17','-O2',str(cpp),'-o',str(exe),'-luser32','-lkernel32']
  build=subprocess.run(cmd,capture_output=True,text=True);(out/f'{target}-build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
  if build.returncode:print(build.stdout+build.stderr);raise SystemExit(build.returncode)
  run=subprocess.run([str(exe)],capture_output=True,text=True,encoding='utf-8');(out/f'{target}.log').write_text(run.stdout+run.stderr,encoding='utf-8')
  results.append(dict(target=target,build_command=cmd,exit=run.returncode,stdout=run.stdout,stderr=run.stderr));print(run.stdout+run.stderr)
  if run.returncode:raise SystemExit(run.returncode)
 (out/'result.json').write_text(json.dumps(dict(scope='Actual source function extraction; isolated stubs; no game process',results=results),indent=2),encoding='utf-8')
if __name__=='__main__':main()
