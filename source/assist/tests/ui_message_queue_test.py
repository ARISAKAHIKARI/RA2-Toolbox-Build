"""Production fair queue with continuously refilled posted/input/paint queues."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=r'''
#pragma once
#include <cstdint>
using UINT=unsigned;using HWND=void*;using WPARAM=uintptr_t;
struct MSG{HWND hwnd=nullptr;UINT message=0;};
constexpr UINT PM_REMOVE=1,QS_TIMER=16,PM_QS_INPUT=1u<<16,PM_QS_PAINT=32u<<16,WM_QUIT=18;
constexpr int FALSE=0;inline bool posted=true,input=true,paint=true,timer=true,quit=false;
inline int PeekMessageW(MSG*m,HWND,UINT,UINT,UINT flags){
 if(quit){quit=false;m->message=WM_QUIT;return 1;}
 UINT mask=flags&0xffff0000u;
 // Win32 unfiltered priority: posted work before input, paint and timers.
 if((!mask&&posted)){m->message=100;return 1;}
 if((!mask||mask==PM_QS_INPUT)&&input){m->message=101;return 1;}
 if((!mask||mask==PM_QS_PAINT)&&paint){m->message=102;return 1;}
 if((!mask||mask==(QS_TIMER<<16))&&timer){m->message=103;return 1;}
 return 0;
}
inline int GetMessageW(MSG*m,HWND h,UINT a,UINT b){return PeekMessageW(m,h,a,b,PM_REMOVE)?(m->message==WM_QUIT?0:1):-1;}
'''
body=r'''
#include "ui_message_queue.h"
#include <cassert>
#include <cstdio>
int main(){uimessages::Queue queue;MSG m;unsigned counts[4]{};
 for(unsigned i=0;i<10000;++i){assert(queue.get(m)==1);assert(m.message>=100&&m.message<=103);++counts[m.message-100];
  if(i%10==9)for(auto n:counts)assert(n>0);}
 for(auto n:counts)assert(n>=2000); // all four classes make progress under flood
 input=paint=timer=false;for(unsigned i=0;i<20;++i){assert(queue.get(m)==1&&m.message==100);}
 quit=true;assert(queue.get(m)==0&&m.message==WM_QUIT);posted=false;assert(queue.get(m)==-1);
 std::puts("PASS 10000 saturated turns: input, paint, timers and posted work all progress; quit/error preserved");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'windows.h').write_text(header);(p/'t.cpp').write_text(body)
 subprocess.run(['g++','-std=c++17','-O2','-I'+d,'-I'+str(root/'shared/source'),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
