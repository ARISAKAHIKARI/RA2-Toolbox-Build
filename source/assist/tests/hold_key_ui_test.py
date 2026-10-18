"""Execute the shipped hotkey subclass against a model of the Win32 hotkey control.
Verifies modifier-only bindings/notifications; not visual/native Windows testing.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'shared/source/modern_ui.h').read_text();s=s[s.index('inline LRESULT CALLBACK hotkeyProc'):s.index('inline LRESULT CALLBACK parentProc')]
prefix=r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <iostream>
#define CALLBACK
using HWND=int;using HANDLE=void*;using HDC=void*;using LRESULT=intptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using UINT=unsigned;using UINT_PTR=uintptr_t;using DWORD_PTR=uintptr_t;
struct PAINTSTRUCT{};
constexpr int WM_NCDESTROY=1,WM_ERASEBKGND=2,WM_PAINT=3,WM_PRINTCLIENT=4,WM_KEYDOWN=5,WM_KEYUP=6,WM_SYSKEYDOWN=7,WM_SYSKEYUP=8,WM_CHAR=9,WM_SETFOCUS=10,WM_KILLFOCUS=11,WM_ENABLE=12,WM_SETFONT=13,WM_SIZE=14,WM_COMMAND=15,HKM_SETHOTKEY=16,HKM_GETHOTKEY=17,EN_CHANGE=18;
constexpr int VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18,FALSE=0,TRUE=1;
std::map<std::pair<int,std::wstring>,HANDLE>props;unsigned nativeKey=0,mods=0,notifications=0;
HANDLE GetPropW(HWND w,const wchar_t*n){return props[{w,n}];}void SetPropW(HWND w,const wchar_t*n,HANDLE h){props[{w,n}]=h;}void RemovePropW(HWND w,const wchar_t*n){props.erase({w,n});}
void RemoveWindowSubclass(... ){}void InvalidateRect(...){}void paintHotkey(...){}HDC BeginPaint(...){return nullptr;}void EndPaint(...){}HWND GetParent(HWND){return 2;}int GetDlgCtrlID(HWND){return 450;}
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
 event(WM_NCDESTROY,0);assert(!GetPropW(1,L"RA2.Modern.Hold"));
 std::cout<<"PASS: bare Ctrl/Shift/Alt, reload, notifications, Ctrl+G, clear, focus loss and teardown using shipped subclass\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(prefix+s+suffix)
 subprocess.run(['g++','-std=c++17','-O1',str(cpp),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
