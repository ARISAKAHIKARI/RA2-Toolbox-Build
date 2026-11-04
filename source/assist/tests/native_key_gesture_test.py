"""Run production Runtime message gesture tracking with controlled Win32 input.

This exercises the existing message boundary's tracking helper, not a game or
an installed Windows hook. It does not certify visual or multiplayer behavior.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'assist/source/native_message_policy.h').read_text()
body = source[source.index('static void recordRuntimeGesture'):source.index('extern "C" int AssistFilterMessage')]
prefix = r'''
#include "key_gesture.h"
#include <cassert>
#include <iostream>
using WPARAM=uintptr_t;using LPARAM=intptr_t;
struct MSG {unsigned message;WPARAM wParam;LPARAM lParam;};
constexpr unsigned WM_KEYDOWN=0x100,WM_KEYUP=0x101,WM_SYSKEYDOWN=0x104,WM_SYSKEYUP=0x105;
constexpr unsigned WM_LBUTTONDOWN=0x201,WM_LBUTTONUP=0x202,WM_LBUTTONDBLCLK=0x203,WM_RBUTTONDOWN=0x204,WM_RBUTTONUP=0x205,WM_RBUTTONDBLCLK=0x206;
constexpr unsigned WM_MBUTTONDOWN=0x207,WM_MBUTTONUP=0x208,WM_MBUTTONDBLCLK=0x209,WM_XBUTTONDOWN=0x20b,WM_XBUTTONUP=0x20c,WM_XBUTTONDBLCLK=0x20d;
constexpr unsigned VK_LBUTTON=1,VK_RBUTTON=2,VK_MBUTTON=4,VK_XBUTTON1=5,VK_XBUTTON2=6,XBUTTON1=1,XBUTTON2=2;
static unsigned HIWORD(uintptr_t value){return unsigned(value>>16)&65535;}
static uint8_t fixtureKeys[256]{};static unsigned reads=0;
static short GetKeyState(unsigned key){++reads;return fixtureKeys[key]?-32768:0;}
static keygesture::Tracker runtimeKeys;
'''
suffix = r'''
int main(){
 using namespace keygesture;constexpr unsigned hold=unsigned(Mode::Hold);
 auto send=[](unsigned message,WPARAM key,uint32_t now,bool active=true,uint32_t epoch=1,LPARAM flags=0){MSG msg{message,key,flags};reads=0;recordRuntimeGesture(&msg,active,epoch,now);assert(reads<=3);};
 struct Mouse{unsigned key,down,up,twice;WPARAM parameter;};
 for(auto mouse:{Mouse{1,WM_LBUTTONDOWN,WM_LBUTTONUP,WM_LBUTTONDBLCLK,0},Mouse{2,WM_RBUTTONDOWN,WM_RBUTTONUP,WM_RBUTTONDBLCLK,0},Mouse{4,WM_MBUTTONDOWN,WM_MBUTTONUP,WM_MBUTTONDBLCLK,0},Mouse{5,WM_XBUTTONDOWN,WM_XBUTTONUP,WM_XBUTTONDBLCLK,WPARAM(1)<<16},Mouse{6,WM_XBUTTONDOWN,WM_XBUTTONUP,WM_XBUTTONDBLCLK,WPARAM(2)<<16}}){
  runtimeKeys.clear();send(mouse.down,mouse.parameter,10);assert(runtimeKeys.qualifies(uint16_t(mouse.key|hold))&&!runtimeKeys.qualifies(uint16_t(mouse.key|hold|Double)));
  send(mouse.down,mouse.parameter,11);assert(!runtimeKeys.qualifies(uint16_t(mouse.key|hold|Double))); // duplicate down is not a second click
  send(mouse.up,mouse.parameter,20);assert(!runtimeKeys.qualifies(uint16_t(mouse.key|hold)));
  send(mouse.twice,mouse.parameter,100);assert(runtimeKeys.qualifies(uint16_t(mouse.key|hold|Double)));
  send(mouse.up,mouse.parameter,110);assert(!runtimeKeys.qualifies(uint16_t(mouse.key|hold|Double)));
 }
 runtimeKeys.clear();send(WM_KEYDOWN,'A',10);send(WM_KEYDOWN,'A',20,true,1,LPARAM(1)<<30);assert(!(runtimeKeys.held['A']&2));send(WM_KEYUP,'A',30);send(WM_KEYDOWN,'A',100);assert(runtimeKeys.qualifies(uint16_t('A'|hold|Double)));
 runtimeKeys.clear();fixtureKeys[17]=1;send(WM_KEYDOWN,17,10);send(WM_KEYDOWN,'X',20);send(WM_KEYUP,'X',30);
 fixtureKeys[17]=0;send(WM_KEYUP,17,40);fixtureKeys[17]=1;send(WM_KEYDOWN,17,50);send(WM_KEYDOWN,'X',60);assert(runtimeKeys.qualifies(uint16_t('X'|0x200|hold|Double)));
 fixtureKeys[17]=0;send(WM_KEYUP,17,70);assert(!runtimeKeys.qualifies(uint16_t('X'|0x200|hold|Double)));fixtureKeys[17]=1;send(WM_KEYDOWN,17,80);assert(!runtimeKeys.qualifies(uint16_t('X'|0x200|hold|Double)));
 memset(fixtureKeys,0,256);runtimeKeys.clear();send(WM_MBUTTONDOWN,0,10);send(WM_MBUTTONUP,0,20);send(0,0,21,false);send(WM_MBUTTONDOWN,0,30);assert(!(runtimeKeys.held[4]&2));send(WM_MBUTTONUP,0,40);send(WM_MBUTTONDOWN,0,50,true,2);assert(!(runtimeKeys.held[4]&2));
 auto serial=runtimeKeys.next;send(WM_XBUTTONDOWN,WPARAM(3)<<16,60,true,2);send(0x20a,0,70,true,2);assert(runtimeKeys.next==serial);
 std::cout<<"PASS production Runtime keyboard and all five mouse gesture edges, DBLCLK/repeat, prefix cancellation, focus/match reset and bounded modifier reads; existing message hook only\n";
}
'''
with tempfile.TemporaryDirectory() as temporary:
    path = Path(temporary)
    cpp, exe = path / 'native-gestures.cpp', path / 'native-gestures'
    cpp.write_text(prefix + body + suffix)
    subprocess.run(['g++', '-std=c++17', '-O1', '-I' + str(root / 'shared/source'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
