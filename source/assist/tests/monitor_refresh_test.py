#!/usr/bin/env python3
"""Exercise the production monitor resolver with controlled Win32 boundaries."""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--header', type=Path, default=Path(__file__).resolve().parents[2] / 'shared/source/monitor_refresh.h')
args = parser.parse_args()

windows = r'''
#pragma once
#include <cstdint>
#include <cwchar>
#include <map>
#include <cassert>
using DWORD=uint32_t;using WORD=uint16_t;using BOOL=int;
using HWND=void*;using HMONITOR=void*;
inline void* handle(uintptr_t n){return reinterpret_cast<void*>(n);}
constexpr DWORD MONITOR_DEFAULTTONEAREST=2,ENUM_CURRENT_SETTINGS=DWORD(-1),DM_DISPLAYFREQUENCY=0x400000;
struct MONITORINFO{DWORD cbSize;};using LPMONITORINFO=MONITORINFO*;
struct MONITORINFOEXW{DWORD cbSize=0;wchar_t szDevice[32]{};};
struct DEVMODEW{WORD dmSize=0,dmDriverExtra=0;DWORD dmFields=0,dmDisplayFrequency=0;};
struct Mode{DWORD hz=60;bool readable=true,info=true,field=true;};
inline std::map<HWND,HMONITOR> windows;inline std::map<HMONITOR,Mode> modes;
inline unsigned windowQueries=0,monitorQueries=0,infoQueries=0,modeQueries=0;
inline HMONITOR queriedMonitor=0;inline HWND queriedWindow=0;
inline BOOL IsWindow(HWND w){++windowQueries;return windows.count(w);}
inline HMONITOR MonitorFromWindow(HWND w,DWORD flags){++monitorQueries;queriedWindow=w;assert(flags==MONITOR_DEFAULTTONEAREST);return windows.at(w);}
inline BOOL GetMonitorInfoW(HMONITOR m,LPMONITORINFO info){
 ++infoQueries;queriedMonitor=m;assert(info->cbSize==sizeof(MONITORINFOEXW));
 auto found=modes.find(m);if(found==modes.end()||!found->second.info)return 0;
 auto out=reinterpret_cast<MONITORINFOEXW*>(info);std::swprintf(out->szDevice,32,L"display-%zu",reinterpret_cast<size_t>(m));return 1;
}
inline BOOL EnumDisplaySettingsExW(const wchar_t* device,DWORD index,DEVMODEW* mode,DWORD flags){
 ++modeQueries;assert(index==ENUM_CURRENT_SETTINGS&&flags==0&&mode->dmSize==sizeof(DEVMODEW)&&mode->dmDriverExtra==0);
 size_t selected=0;assert(std::swscanf(device,L"display-%zu",&selected)==1);assert(handle(selected)==queriedMonitor);
 auto found=modes.find(handle(selected));if(found==modes.end()||!found->second.readable)return 0;
 mode->dmFields=found->second.field?DM_DISPLAYFREQUENCY:0;mode->dmDisplayFrequency=found->second.hz;return 1;
}
'''

scenario = r'''
#include @HEADER@
#include <iostream>
int main(){
 using namespace monitorrefresh;
 const auto game=handle(10),ui=handle(20),nextGame=handle(30),invalid=handle(999),m1=handle(1),m2=handle(2),m3=handle(3);
 windows={{game,m1},{ui,m2},{nextGame,m1}};modes={{m1,{144}},{m2,{60}},{m3,{240}}};
 Tracker t;assert(t.rate()==60&&modeQueries==0);
 assert(t.update(game,ui,0)==144&&queriedWindow==game&&queriedMonitor==m1&&modeQueries==1);
 const unsigned queried=modeQueries;
 for(DWORD now=1;now<1000;++now)assert(t.update(game,ui,now)==144);
 assert(modeQueries==queried&&monitorQueries==1000);
 // Reading the resolved value on a high-frequency draw path is a pure load.
 unsigned beforeWindow=windowQueries,beforeMonitor=monitorQueries,beforeInfo=infoQueries;
 for(int n=0;n<10000;++n)assert(t.rate()==144);
 assert(windowQueries==beforeWindow&&monitorQueries==beforeMonitor&&infoQueries==beforeInfo&&modeQueries==queried);
 // A same-HWND move to a different monitor bypasses the one-second budget.
 windows[game]=m3;assert(t.update(game,ui,999)==240&&queriedMonitor==m3&&modeQueries==queried+1);
 // A new game HWND on the same monitor is sampled immediately as well.
 windows[nextGame]=m3;assert(t.update(nextGame,ui,999)==240&&modeQueries==queried+2);
 // Same-monitor frequency changes are eventually seen without notifications.
 modes[m3].hz=165;assert(t.update(nextGame,ui,1998)==240);assert(t.update(nextGame,ui,1999)==165);
 modes[m3].hz=120;t.invalidate();assert(t.update(nextGame,ui,2000)==120);
 modes[m3].hz=360;assert(t.update(nextGame,ui,2001,true)==360);
 // No valid game window uses the supplied UI monitor, never the primary DC.
 assert(t.update(invalid,ui,2002)==60&&queriedWindow==ui&&queriedMonitor==m2);
 modes[m2].hz=75;windows.erase(ui);assert(t.update(invalid,ui,2003)==60);
 windows[ui]=m2;assert(t.update(0,ui,2004)==75);
 // Drivers may return unknown/default frequency, missing fields, or failure.
 for(DWORD hz:{0u,1u,2u,4u}){modes[m2].hz=hz;assert(t.update(0,ui,2100,true)==60);}
 for(DWORD hz:{5u,24u,59u,60u,144u,165u,240u,360u,500u}){modes[m2].hz=hz;assert(t.update(0,ui,2101,true)==int(hz));}
 for(DWORD hz:{501u,600u,1000u,0xffffffffu}){modes[m2].hz=hz;assert(t.update(0,ui,2102,true)==500);}
 modes[m2].hz=144;modes[m2].field=false;assert(t.update(0,ui,2200,true)==60);modes[m2].field=true;
 modes[m2].readable=false;assert(t.update(0,ui,2201,true)==60);
 unsigned failedQueries=modeQueries;assert(t.update(0,ui,3200)==60&&modeQueries==failedQueries);
 modes[m2].readable=true;assert(t.update(0,ui,3201)==144&&modeQueries==failedQueries+1);
 modes[m2].info=false;assert(t.update(0,ui,3202,true)==60);modes[m2].info=true;
 windows[ui]=0;assert(t.update(0,ui,3203)==60);windows[ui]=m2;
 // GetTickCount wrap retains the one-second interval without a retry storm.
 Tracker wrapping;assert(wrapping.update(0,ui,0xfffffff0u)==144);
 modes[m2].hz=240;assert(wrapping.update(0,ui,0x20u)==144);assert(wrapping.update(0,ui,0x3d8u)==240);
 assert(periodMicros(5)==200000&&periodMicros(60)==16666&&periodMicros(144)==6944&&periodMicros(165)==6060);
 assert(periodMicros(240)==4166&&periodMicros(360)==2777&&periodMicros(500)==2000&&periodMicros(1000)==2000);
 assert(periodMicros(0)==16666&&periodMicros(-1)==16666);
 std::cout<<"PASS production monitor refresh: game/UI selection, same-HWND monitor moves, mode changes, force/invalidate, bounded retry, invalid/default fields, 500 Hz cap, wrap and pure cached reads; microsecond periods\n";
}
'''

with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    (temp / 'windows.h').write_text(windows)
    (temp / 'test.cpp').write_text(scenario.replace('@HEADER@', json.dumps(str(args.header.resolve()))))
    subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I', str(temp), str(temp / 'test.cpp'), '-o', str(temp / 'test')], check=True)
    subprocess.run([str(temp / 'test')], check=True)
