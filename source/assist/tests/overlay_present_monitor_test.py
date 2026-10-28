#!/usr/bin/env python3
"""Exercise production compositor monitor resolution with Win32 boundaries.

The fixture reuses the existing monitor test's native boundary, without running
that script. Real-HWND current-mode/fallback coverage is in the Windows test.
"""
from pathlib import Path
import ast
import json
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
tree=ast.parse((root/'assist/tests/monitor_refresh_test.py').read_text())
windows=next(ast.literal_eval(n.value) for n in tree.body if isinstance(n,ast.Assign)
             and any(isinstance(t,ast.Name) and t.id=='windows' for t in n.targets))
scenario=r'''
#include @HEADER@
#include <iostream>
int main(){
 using namespace overlaypresentrate;
 auto game=handle(10),nextGame=handle(20),m1=handle(1),m2=handle(2);
 windows={{game,m1},{nextGame,m2}};modes={{m1,{144}},{m2,{60}}};
 Monitor probe;assert(probe.rate()==0&&modeQueries==0);
 assert(probe.update(game,0)==144&&limit(500,probe.rate())==144&&modeQueries==1);
 for(DWORD now=1;now<1000;++now)assert(probe.update(game,now)==144);
 assert(modeQueries==1&&monitorQueries==1000);
 auto queries=modeQueries;auto reads=monitorQueries;
 for(int n=0;n<10000;++n)assert(probe.rate()==144&&limit(120,probe.rate())==120);
 assert(modeQueries==queries&&monitorQueries==reads);
 windows[game]=m2;assert(probe.update(game,999)==60&&modeQueries==queries+1);
 assert(probe.update(nextGame,999)==60&&modeQueries==queries+2);
 modes[m2].hz=165;assert(probe.update(nextGame,1998)==60&&modeQueries==queries+2);
 assert(probe.update(nextGame,1999)==165&&modeQueries==queries+3);
 modes[m2].hz=240;probe.invalidate();assert(probe.update(nextGame,2000)==240);
 for(DWORD hz:{0u,1u,2u,4u}){modes[m2].hz=hz;probe.invalidate();assert(probe.update(nextGame,2100)==0&&limit(500,probe.rate())==500);}
 modes[m2].hz=144;modes[m2].field=false;probe.invalidate();assert(probe.update(nextGame,2200)==0);modes[m2].field=true;
 modes[m2].readable=false;probe.invalidate();assert(probe.update(nextGame,2201)==0);
 queries=modeQueries;assert(probe.update(nextGame,3200)==0&&modeQueries==queries);
 modes[m2].readable=true;assert(probe.update(nextGame,3201)==144&&modeQueries==queries+1);
 modes[m2].info=false;probe.invalidate();assert(probe.update(nextGame,3300)==0);modes[m2].info=true;
 assert(probe.update(nullptr,3301)==0&&limit(360,probe.rate())==360);
 windows.erase(game);assert(probe.update(game,3302)==0);
 Monitor wrapping;assert(wrapping.update(nextGame,0xfffffff0u)==144);
 modes[m2].hz=240;assert(wrapping.update(nextGame,0x20u)==144&&wrapping.update(nextGame,0x3d8u)==240);
 std::cout<<"PASS: compositor-only known-Hz resolver; 1s mode-query throttle, immediate monitor/window/display invalidation, unknown preserves request, bounded failure retry, tick wrap and pure cached reads\n";
}
'''
with tempfile.TemporaryDirectory(prefix='overlay-present-monitor-') as folder:
    temp=Path(folder)
    (temp/'windows.h').write_text(windows)
    (temp/'test.cpp').write_text(scenario.replace('@HEADER@',json.dumps(str(root/'shared/source/overlay_present_monitor.h'))))
    subprocess.run(['g++','-std=c++17','-O2','-Wall','-Wextra','-I',str(temp),str(temp/'test.cpp'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
