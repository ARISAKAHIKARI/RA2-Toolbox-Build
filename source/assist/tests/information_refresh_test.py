#!/usr/bin/env python3
"""Execute the production information-rate integration with the real resolver.

The two functions are extracted from assist/main.cpp; only the Win32 boundary,
UI notifications and frame-clock sink are doubled. Both suite and standalone
fallback paths are exercised. This does not validate hardware presentation.
"""
from pathlib import Path
import argparse
import ast
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--main', type=Path, default=root / 'assist/source/main.cpp')
parser.add_argument('--header', type=Path, default=root / 'shared/source/monitor_refresh.h')
args = parser.parse_args()


def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


source = args.main.read_text()
rate = function(source, 'static int informationFPS()')
eligibility = function(source, 'static bool displayFrameRequested()')
sync = function(source, 'static void syncInformationRate(bool force=false)')
# Share the tested Win32 boundary instead of maintaining a second model of
# monitor/device selection. literal_eval never runs that script or its CLI.
boundary = ast.parse((root / 'assist/tests/monitor_refresh_test.py').read_text())
windows = next(ast.literal_eval(node.value) for node in boundary.body
               if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'windows' for t in node.targets))

prefix = r'''
#include @HEADER@
#include <iostream>
#include <string>
#include <vector>
static HWND game=handle(10),ui=handle(20),rateEdit=handle(310);
static bool infoAdaptive=false,enabled=true,focused=true,haveStatus=true;
static int fps=120,paintRevision=0,clockRate=0;
static bool clockActive=false,secondaryAvailable=false;
static unsigned clockCalls=0,publishes=0,enableCalls=0,textCalls=0,tickCalls=0,controlCalls=0;
static DWORD clockNow=0;
static struct {int renderStatus=1;} snap;
static monitorrefresh::Tracker informationMonitor;
static std::map<HWND,bool> available;
static std::map<HWND,std::wstring> texts;
#ifdef RA2_UNIFIED
namespace suite {static HWND frame=handle(30);static HWND parent(){return frame;}}
#endif
static DWORD GetTickCount(){++tickCalls;return clockNow;}
static bool foregroundGame(){return focused;}
static bool secondaryDisplayAvailable(){return secondaryAvailable;}
@ELIGIBILITY@
static void clockState(bool active,int hz){++clockCalls;clockActive=active;clockRate=hz;}
static void EnableWindow(HWND h,bool on){assert(h==rateEdit&&h);++enableCalls;available[h]=on;}
static HWND GetDlgItem(HWND window,int id){assert(window==ui&&id==9161);++controlCalls;return haveStatus?handle(id):nullptr;}
static void SetWindowTextW(HWND h,const wchar_t* text){assert(h==handle(9161));++textCalls;texts[h]=text;}
@RATE@
struct Publication{int manual,effective;bool adaptive;};
static std::vector<Publication> published;
static void publishSuiteDisplay(){++publishes;published.push_back({fps,informationFPS(),infoAdaptive});}
@SYNC@
'''

scenario = r'''
int main(){
 const auto m1=handle(1),m2=handle(2),m3=handle(3),m4=handle(4);
 windows={{game,m1},{ui,m2},{handle(30),m3}};modes={{m1,{60}},{m2,{75}},{m3,{90}},{m4,{240}}};
 assert(informationFPS()==120);
 fps=1;assert(informationFPS()==5&&fps==1);fps=900;assert(informationFPS()==500&&fps==900);fps=120;
 syncInformationRate(true);
 assert(clockActive&&clockRate==120&&available[rateEdit]&&texts[handle(9161)]==L"当前：120 Hz");
 assert(publishes==1&&paintRevision==1&&modeQueries==0&&monitorQueries==0&&tickCalls==0);
 assert(published.back().manual==120&&published.back().effective==120&&!published.back().adaptive);
 unsigned oldPublish=publishes,oldEnable=enableCalls,oldText=textCalls;
 for(int n=0;n<100;++n){++clockNow;syncInformationRate();}
 assert(publishes==oldPublish&&enableCalls==oldEnable&&textCalls==oldText&&modeQueries==0&&monitorQueries==0&&tickCalls==0);

 // The first automatic update resolves the game's 60 Hz screen, independently
 // of the retained manual 120 Hz and the tool UI's 75/90 Hz screen.
 infoAdaptive=true;syncInformationRate();
 assert(informationFPS()==60&&clockRate==60&&clockActive&&fps==120&&!available[rateEdit]);
 assert(queriedWindow==game&&queriedMonitor==m1&&modeQueries==1);
 assert(published.back().manual==120&&published.back().effective==60&&published.back().adaptive);
 unsigned oldWindows=windowQueries,oldMonitors=monitorQueries,oldModes=modeQueries,oldTicks=tickCalls;
 oldPublish=publishes;oldEnable=enableCalls;oldText=textCalls;unsigned oldControls=controlCalls;
 for(int n=0;n<10000;++n)assert(informationFPS()==60);
 assert(windowQueries==oldWindows&&monitorQueries==oldMonitors&&modeQueries==oldModes&&tickCalls==oldTicks);
 assert(publishes==oldPublish&&enableCalls==oldEnable&&textCalls==oldText&&controlCalls==oldControls);

 modes[m1].hz=144;clockNow+=1000;syncInformationRate();
 assert(clockRate==144&&informationFPS()==144&&fps==120&&texts[handle(9161)]==L"当前：144 Hz"&&publishes==oldPublish+1);
 oldPublish=publishes;oldModes=modeQueries;
 modes[m1].hz=165;++clockNow;syncInformationRate();
 assert(informationFPS()==144&&publishes==oldPublish&&modeQueries==oldModes);
 informationMonitor.invalidate();syncInformationRate();
 assert(clockRate==165&&informationFPS()==165&&fps==120&&texts[handle(9161)]==L"当前：165 Hz"&&publishes==oldPublish+1);
 windows[game]=m4;++clockNow;syncInformationRate();assert(clockRate==240&&informationFPS()==240&&queriedMonitor==m4);

 // Forced synchronization repairs UI state even with an unchanged target.
 available[rateEdit]=true;texts[handle(9161)]=L"stale";oldPublish=publishes;oldEnable=enableCalls;oldText=textCalls;
 syncInformationRate(true);
 assert(!available[rateEdit]&&texts[handle(9161)]==L"当前：240 Hz"&&publishes==oldPublish+1&&enableCalls==oldEnable+1&&textCalls==oldText+1);
 oldPublish=publishes;oldModes=modeQueries;oldEnable=enableCalls;oldText=textCalls;
 for(int n=0;n<100;++n){++clockNow;syncInformationRate();}
 assert(publishes==oldPublish&&modeQueries==oldModes&&enableCalls==oldEnable&&textCalls==oldText);
 // Clock eligibility still follows foreground, rendering and module state.
 focused=false;syncInformationRate();assert(!clockActive&&clockRate==240);
 focused=true;enabled=false;syncInformationRate();assert(!clockActive);
 secondaryAvailable=true;focused=false;syncInformationRate();assert(clockActive&&clockRate==240);
 snap.renderStatus=0;syncInformationRate();assert(!clockActive);
 snap.renderStatus=1;secondaryAvailable=false;syncInformationRate();assert(!clockActive);focused=true;
 enabled=true;snap.renderStatus=0;syncInformationRate();assert(!clockActive);
 snap.renderStatus=1;syncInformationRate();assert(clockActive);
 assert(publishes==oldPublish);

 // Turning auto off restores the manual value without a monitor read.
 oldMonitors=monitorQueries;oldModes=modeQueries;oldTicks=tickCalls;
 infoAdaptive=false;syncInformationRate();
 assert(informationFPS()==120&&clockRate==120&&fps==120&&available[rateEdit]);
 assert(monitorQueries==oldMonitors&&modeQueries==oldModes&&tickCalls==oldTicks);
 assert(published.back().manual==120&&published.back().effective==120&&!published.back().adaptive);
 fps=90;syncInformationRate(true);assert(clockRate==90&&fps==90&&modeQueries==oldModes);
 // Mode changes must update enablement/publication even if Hz is identical.
 modes[m4].hz=90;clockNow+=1000;oldPublish=publishes;infoAdaptive=true;syncInformationRate();
 assert(clockRate==90&&fps==90&&!available[rateEdit]&&publishes==oldPublish+1);
 oldPublish=publishes;oldModes=modeQueries;infoAdaptive=false;syncInformationRate();
 assert(clockRate==90&&available[rateEdit]&&publishes==oldPublish+1&&modeQueries==oldModes);

 // A missing game uses the correct UI owner in each build configuration.
 game=handle(999);focused=false;infoAdaptive=true;++clockNow;syncInformationRate();
#ifdef RA2_UNIFIED
 assert(informationFPS()==90&&queriedWindow==suite::frame&&queriedMonitor==m3);
 suite::frame=nullptr;syncInformationRate();assert(informationFPS()==75&&queriedWindow==ui);
#else
 assert(informationFPS()==75&&queriedWindow==ui&&queriedMonitor==m2);
#endif
 assert(!clockActive&&fps==90);
 game=handle(10);modes[m4].hz=165;focused=true;syncInformationRate();
 assert(informationFPS()==165&&clockActive&&queriedWindow==game&&fps==90);
 // Startup/shutdown controls may be absent; forced synchronization is safe.
 rateEdit=nullptr;haveStatus=false;oldEnable=enableCalls;oldText=textCalls;syncInformationRate(true);
 assert(enableCalls==oldEnable&&textCalls==oldText&&clockRate==165&&fps==90);
#ifdef RA2_UNIFIED
 std::cout<<"PASS production information refresh (suite): ";
#else
 std::cout<<"PASS production information refresh (standalone): ";
#endif
 std::cout<<"manual/auto 60/144/165 transitions, retained manual value, owner fallback, force/no-op controls, clock eligibility and no per-frame Win32 queries\n";
}
'''

with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    (temp / 'windows.h').write_text(windows)
    code = prefix.replace('@HEADER@', json.dumps(str(args.header.resolve()))).replace('@RATE@', rate).replace('@SYNC@', sync).replace('@ELIGIBILITY@', eligibility) + scenario
    (temp / 'test.cpp').write_text(code)
    for unified in (False, True):
        binary = temp / ('suite' if unified else 'standalone')
        flags = ['-DRA2_UNIFIED'] if unified else []
        subprocess.run(['g++', '-std=c++17', '-O2', '-Wall', '-Wextra', *flags, '-I', str(temp), str(temp / 'test.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
