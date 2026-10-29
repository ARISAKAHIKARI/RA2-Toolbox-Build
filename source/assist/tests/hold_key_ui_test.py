"""Execute the shipped hotkey subclass against a model of the Win32 hotkey control.
Verifies modifier-only bindings/notifications; not visual/native Windows testing.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=(root/'shared/source/modern_ui.h').read_text();s=header[header.index('inline LRESULT CALLBACK hotkeyProc'):header.index('inline LRESULT CALLBACK parentProc')]
menu=header[header.index('enum HotkeyMenu'):header.index('inline std::wstring hotkeyName')]
command=header[header.index('inline bool hotkeyCommand'):header.index('inline void showHotkeyMenu')]
prefix=r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <iostream>
#include "key_gesture.h"
#define CALLBACK
using HWND=int;using HANDLE=void*;using HDC=void*;using LRESULT=intptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using UINT=unsigned;using UINT_PTR=uintptr_t;using DWORD_PTR=uintptr_t;
struct PAINTSTRUCT{};struct RECT{int left=0,top=0,right=200,bottom=30;};void GetClientRect(int,RECT*r){*r={};}int scale(int x,int){return x;}int windowDpi(int){return 96;}unsigned LOWORD(intptr_t x){return x&65535;}
constexpr int WM_LBUTTONDOWN=40,WM_RBUTTONUP=41,WM_CONTEXTMENU=42,WM_MBUTTONDOWN=43,WM_XBUTTONDOWN=44;
constexpr int WM_NCDESTROY=1,WM_ERASEBKGND=2,WM_PAINT=3,WM_PRINTCLIENT=4,WM_KEYDOWN=5,WM_KEYUP=6,WM_SYSKEYDOWN=7,WM_SYSKEYUP=8,WM_CHAR=9,WM_SETFOCUS=10,WM_KILLFOCUS=11,WM_ENABLE=12,WM_SETFONT=13,WM_SIZE=14,WM_COMMAND=15,HKM_SETHOTKEY=16,HKM_GETHOTKEY=17,EN_CHANGE=18;
constexpr int VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18,VK_APPS=93,VK_F10=121,XBUTTON1=1,FALSE=0,TRUE=1;
std::map<std::pair<int,std::wstring>,HANDLE>props;unsigned nativeKey=0,mods=0,notifications=0,menus=0;bool enabled=true;
HANDLE GetPropW(HWND w,const wchar_t*n){return props[{w,n}];}void SetPropW(HWND w,const wchar_t*n,HANDLE h){props[{w,n}]=h;}void RemovePropW(HWND w,const wchar_t*n){props.erase({w,n});}
void RemoveWindowSubclass(... ){}void InvalidateRect(...){}void paintHotkey(...){}HDC BeginPaint(...){return nullptr;}void EndPaint(...){}HWND GetParent(HWND){return 2;}int GetDlgCtrlID(HWND){return 450;}
bool IsWindowEnabled(HWND){return enabled;}void SetFocus(HWND){}unsigned HIWORD(uintptr_t value){return unsigned(value>>16)&65535;}short GetKeyState(int vk){unsigned bit=vk==16?1:vk==17?2:vk==18?4:0;return mods&bit?-1:0;}
void refreshHotkey(HWND){}void showHotkeyMenu(HWND,LPARAM=-1){++menus;}
WPARAM MAKEWPARAM(int lo,int hi){return unsigned(lo)|(unsigned(hi)<<16);}
LRESULT hotkeyProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
LRESULT SendMessageW(HWND w,UINT m,WPARAM wp,LPARAM lp){if(w==2){assert(m==WM_COMMAND&&(wp>>16)==EN_CHANGE);++notifications;return 0;}return hotkeyProc(w,m,wp,lp,1,0);}
LRESULT DefSubclassProc(HWND,UINT m,WPARAM wp,LPARAM){
 bool modifier=wp==VK_SHIFT||wp==VK_CONTROL||wp==VK_MENU;unsigned bit=wp==VK_SHIFT?1:wp==VK_CONTROL?2:4;
 if(m==HKM_GETHOTKEY)return nativeKey;
 if(m==HKM_SETHOTKEY){nativeKey=modifier?0:wp;return 0;}
 if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN){if(modifier){mods|=bit;nativeKey=0;}else nativeKey=wp=='\b'||wp==46?0:wp|(mods<<8);}
 if(m==WM_KEYUP||m==WM_SYSKEYUP){if(modifier)mods&=~bit;}
 return 0;
}
'''
suffix=r'''
int main(){SetPropW(1,L"RA2.Modern.Hold",(HANDLE)1);
 auto event=[](UINT m,WPARAM wp){return hotkeyProc(1,m,wp,0,1,0);};
 for(int vk:{VK_SHIFT,VK_CONTROL,VK_MENU}){unsigned before=notifications;event(vk==VK_MENU?WM_SYSKEYDOWN:WM_KEYDOWN,vk);event(vk==VK_MENU?WM_SYSKEYUP:WM_KEYUP,vk);assert(event(HKM_GETHOTKEY,0)==vk&&notifications==before+1);event(WM_KILLFOCUS,0);assert(event(HKM_GETHOTKEY,0)==vk);event(HKM_SETHOTKEY,vk);assert(event(HKM_GETHOTKEY,0)==vk);}
 event(WM_KEYDOWN,VK_CONTROL);event(WM_KEYDOWN,'G');event(WM_KEYUP,'G');event(WM_KEYUP,VK_CONTROL);assert(event(HKM_GETHOTKEY,0)==('G'|512));
 event(WM_KEYDOWN,46);event(WM_KEYUP,46);assert(event(HKM_GETHOTKEY,0)==0);
 event(HKM_SETHOTKEY,VK_SHIFT);event(HKM_SETHOTKEY,0);assert(event(HKM_GETHOTKEY,0)==0);
 event(WM_KEYDOWN,VK_CONTROL);event(WM_KILLFOCUS,0);event(WM_KEYUP,VK_CONTROL);assert(event(HKM_GETHOTKEY,0)==0);
 event(HKM_SETHOTKEY,0x8000|'G'|512);assert(event(HKM_GETHOTKEY,0)==(0x8000|'G'|512));
 event(WM_RBUTTONUP,0);assert(event(HKM_GETHOTKEY,0)==(0x8000|'G'|512)&&menus==1);
 assert(!hotkeyCommand(1,KeyRelease)&&!hotkeyCommand(1,KeyPress));
 assert(hotkeyCommand(1,KeyHold));assert(event(HKM_GETHOTKEY,0)==(0x3000|'G'|512));
 assert(hotkeyCommand(1,KeyDoubleHold));assert(event(HKM_GETHOTKEY,0)==(0xb000|'G'|512));
 RemovePropW(1,L"RA2.Modern.Hold");assert(hotkeyCommand(1,KeyDoubleRelease));assert(event(HKM_GETHOTKEY,0)==(0xa000|'G'|512));
 enabled=false;auto old=event(HKM_GETHOTKEY,0);assert(!hotkeyCommand(1,KeyClear));event(WM_KEYDOWN,'H');assert(event(HKM_GETHOTKEY,0)==old);enabled=true;
 SetPropW(1,L"RA2.Modern.PressOnly",(HANDLE)1);assert(!hotkeyCommand(1,KeyRelease)&&hotkeyCommand(1,KeyPress));RemovePropW(1,L"RA2.Modern.PressOnly");
 event(HKM_SETHOTKEY,0x8000|17);assert(event(HKM_GETHOTKEY,0)==(0x8000|17));
 event(WM_NCDESTROY,0);assert(!GetPropW(1,L"RA2.Modern.Hold"));
 std::cout<<"PASS: bare Ctrl/Shift/Alt, reload, notifications, Ctrl+G, clear, focus loss and teardown using shipped subclass\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(prefix+menu+command+s+suffix)
 subprocess.run(['g++','-std=c++17','-O1','-I'+str(root/'shared/source'),str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
