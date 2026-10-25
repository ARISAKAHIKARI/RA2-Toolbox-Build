#!/usr/bin/env python3
"""Run the production alternate-monitor resolver against Win32 boundary fixtures."""
from pathlib import Path
import argparse
import json
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--header', type=Path, default=Path(__file__).resolve().parents[2] / 'shared/source/secondary_display.h')
args = parser.parse_args()

windows = r'''
#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <cassert>
#define CALLBACK
using DWORD=uint32_t;using LONG=int32_t;using BOOL=int;using LPARAM=intptr_t;
using HWND=void*;using HMONITOR=void*;using HDC=void*;
struct RECT{LONG left=0,top=0,right=0,bottom=0;};using LPRECT=RECT*;
struct MONITORINFO{DWORD cbSize=0;RECT rcMonitor{},rcWork{};DWORD dwFlags=0;};
constexpr DWORD MONITOR_DEFAULTTONEAREST=2;constexpr BOOL TRUE=1;
inline void* handle(uintptr_t n){return reinterpret_cast<void*>(n);}
struct Display {RECT bounds{},work{};bool readable=true;};
inline std::map<HWND,HMONITOR> windows;
inline std::map<HMONITOR,Display> displays;
inline std::vector<HMONITOR> order;
inline bool enumerateOk=true;inline unsigned enumerations=0,monitorQueries=0,infoQueries=0;
inline BOOL IsWindow(HWND w){return windows.count(w);}
inline HMONITOR MonitorFromWindow(HWND w,DWORD flags){++monitorQueries;assert(flags==MONITOR_DEFAULTTONEAREST);return windows.at(w);}
inline BOOL GetMonitorInfoW(HMONITOR monitor,MONITORINFO* info){
 ++infoQueries;assert(info->cbSize==sizeof(MONITORINFO));
 auto found=displays.find(monitor);if(found==displays.end()||!found->second.readable)return 0;
 info->rcMonitor=found->second.bounds;info->rcWork=found->second.work;return 1;
}
using MONITORENUMPROC=BOOL (*)(HMONITOR,HDC,LPRECT,LPARAM);
inline BOOL EnumDisplayMonitors(HDC dc,LPRECT clip,MONITORENUMPROC proc,LPARAM parameter){
 ++enumerations;assert(!dc&&!clip);
 for(auto monitor:order){RECT bounds=displays[monitor].bounds;if(!proc(monitor,nullptr,&bounds,parameter))return 0;}
 return enumerateOk;
}
'''

scenario = r'''
#include @HEADER@
#include <iostream>
static bool same(const RECT&a,const RECT&b){return a.left==b.left&&a.top==b.top&&a.right==b.right&&a.bottom==b.bottom;}
int main(){
 using namespace secondarydisplay;
 auto game=handle(10),ui=handle(20),invalid=handle(999),m1=handle(1),m2=handle(2),m3=handle(3);
 windows={{game,m1},{ui,m2}};
 displays={{m1,{{0,0,1920,1080},{0,0,1920,1040}}},
           {m2,{{-2560,-200,0,1240},{-2560,-200,0,1200}}},
           {m3,{{1920,-1080,3840,0},{1920,-1080,3840,-40}}}};
 // One display creates no duplicate; invalidation admits the newly attached
 // alternate without placing any panel back onto the game's display.
 Monitor monitor;order={m1};assert(!monitor.get(game,ui,0).available()&&enumerations==1);
 order={m1,m2};assert(!monitor.get(game,ui,999).available()&&enumerations==1);
 monitor.invalidate();auto target=monitor.get(game,ui,999);
 assert(target.available()&&target.monitor==m2&&same(target.work,displays[m2].work));
 unsigned scans=enumerations,infos=infoQueries;
 for(DWORD tick=1000;tick<1999;++tick)assert(monitor.get(game,ui,tick).monitor==m2);
 assert(enumerations==scans&&infoQueries==infos);
 // Game movement uses the same HWND but immediately selects a new alternate.
 windows[game]=m2;target=monitor.get(game,ui,1998);assert(target.monitor==m1&&enumerations==scans+1);
 // The valid game window takes priority over the toolbox fallback.
 windows[ui]=m1;assert(monitor.get(game,ui,1998).monitor==m1);
 // At most once each second, refresh work area changes without a topology event.
 displays[m1].work.bottom=1000;
 assert(monitor.get(game,ui,2997).work.bottom==1040);
 assert(monitor.get(game,ui,2998).work.bottom==1000);
 // Invalid game handle falls back to UI; no valid anchor cannot label any
 // monitor as secondary, even if other monitors happen to exist.
 target=monitor.get(invalid,ui,2999);assert(target.monitor==m2);
 windows.erase(ui);assert(!monitor.get(invalid,ui,3000).available());
 windows[ui]=m1;
 // Deterministic choice does not depend on monitor enumeration order.
 order={m3,m1,m2};monitor.invalidate();assert(monitor.get(nullptr,ui,3001).monitor==m2);
 order={m2,m3,m1};monitor.invalidate();assert(monitor.get(nullptr,ui,3002).monitor==m2);
 // Hot-unplug and all driver failures clear the alternate; no stale fallback.
 order={m1};monitor.invalidate();assert(!monitor.get(nullptr,ui,3003).available());
 // A mirrored pseudo-monitor occupying the same desktop pixels is not an
 // additional screen even when EnumDisplayMonitors gives it another handle.
 auto mirror=handle(4);displays[mirror]=displays[m1];order={m1,mirror};
 monitor.invalidate();assert(!monitor.get(nullptr,ui,3003).available());
 order={m2,m1};enumerateOk=false;monitor.invalidate();assert(!monitor.get(nullptr,ui,3004).available());enumerateOk=true;
 // Missing the anchor from an enumeration is also an incomplete result.
 order={m2};monitor.invalidate();assert(!monitor.get(nullptr,ui,3005).available());order={m1,m2};
 displays[m2].readable=false;monitor.invalidate();assert(!monitor.get(nullptr,ui,3006).available());displays[m2].readable=true;
 displays[m2].work={-1,0,-2,100};monitor.invalidate();assert(!monitor.get(nullptr,ui,3007).available());
 // Clamp work areas to actual monitor bounds and preserve negative coordinates.
 displays[m2].work={-9999,-9999,100,9999};monitor.invalidate();target=monitor.get(nullptr,ui,3008);
 assert(target.monitor==m2&&same(target.work,displays[m2].bounds));
 displays[m2].work=displays[m2].bounds;
 // Wraparound and cached empty results do not cause a 500 Hz enumeration storm.
 Monitor wrapping;target=wrapping.get(nullptr,ui,0xfffffff0u);assert(target.monitor==m2);scans=enumerations;
 order={m1};assert(wrapping.get(nullptr,ui,0x20u).monitor==m2&&enumerations==scans);
 assert(!wrapping.get(nullptr,ui,0x3d8u).available()&&enumerations==scans+1);
 for(DWORD tick=0x3d9u;tick<0x7c0u;++tick)assert(!wrapping.get(nullptr,ui,tick).available());
 assert(enumerations==scans+1);
 std::cout<<"PASS secondary monitor resolution: single/dual displays, signed work areas, deterministic selection, same-HWND moves, fallback, cached recheck, hotplug/invalidation, partial/failed enumeration, driver/rectangle failures and tick wrap\n";
}
'''

with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    (temp / 'windows.h').write_text(windows)
    (temp / 'test.cpp').write_text(scenario.replace('@HEADER@', json.dumps(str(args.header.resolve()))))
    subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-I', str(temp), str(temp / 'test.cpp'), '-o', str(temp / 'test')], check=True)
    subprocess.run([str(temp / 'test')], check=True)
