"""Execute production UI subclass routing with Win32/GDI doubles (not raster QA)."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=r'''
#pragma once
#include <cstdint>
#include <cwchar>
#include <map>
#include <string>
#include <cassert>
#include <vector>
using LONG=long;using UINT=unsigned;using UINT_PTR=uintptr_t;using DWORD_PTR=uintptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;using LONG_PTR=intptr_t;using COLORREF=uint32_t;
using HANDLE=void*;using HFONT=void*;using HBRUSH=void*;using HDC=void*;using HGDIOBJ=void*;
struct Win;using HWND=Win*;using Proc=LRESULT(*)(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);using BOOL=int;
#define CALLBACK
#define RGB(r,g,b) (COLORREF((r)|((g)<<8)|((b)<<16)))
#define HIWORD(x) (((x)>>16)&65535)
#define _wcsicmp wcscasecmp
constexpr auto HOTKEY_CLASSW=L"msctls_hotkey32";
constexpr int FALSE=0,TRUE=1,WM_APP=32768,BS_TYPEMASK=15,BS_PUSHBUTTON=0,BS_DEFPUSHBUTTON=1,BS_CHECKBOX=2,BS_AUTOCHECKBOX=3,BS_OWNERDRAW=11,GWL_STYLE=-16,WS_TABSTOP=0x10000;
constexpr UINT WM_NCDESTROY=100,WM_PAINT=101,WM_PRINTCLIENT=102,WM_COMMAND=103,WM_DRAWITEM=104,WM_CTLCOLORSTATIC=105,WM_CTLCOLOREDIT=106,WM_CTLCOLORLISTBOX=107,WM_GETFONT=108,WM_SETFONT=109,WM_SETTEXT=110,WM_ENABLE=111,WM_SETFOCUS=112,WM_GETDLGCODE=113,WM_KILLFOCUS=114,BM_GETCHECK=201,BM_SETCHECK=202,BM_SETSTATE=203,BM_GETSTATE=204,BM_SETSTYLE=205;
constexpr UINT BST_CHECKED=1,BST_UNCHECKED=0,BST_PUSHED=4,ODS_DISABLED=1,ODS_FOCUS=2,ODS_SELECTED=4,ODS_NOFOCUSRECT=8,ODT_BUTTON=4,BN_CLICKED=0,DLGC_DEFPUSHBUTTON=16,DLGC_UNDEFPUSHBUTTON=32;
constexpr int NULL_PEN=1,DC_BRUSH=2,TRANSPARENT=1,OPAQUE=2,COLOR_WINDOW=5,COLOR_WINDOWTEXT=8,COLOR_GRAYTEXT=17,FW_NORMAL=400,FW_SEMIBOLD=600,DEFAULT_CHARSET=1,CLEARTYPE_QUALITY=5;
constexpr UINT HKM_GETHOTKEY=300,HKM_SETHOTKEY=301,WM_ERASEBKGND=302,WM_KEYDOWN=303,WM_KEYUP=304,WM_SYSKEYDOWN=305,WM_SYSKEYUP=306,WM_CHAR=307,WM_SIZE=308;
constexpr unsigned HOTKEYF_CONTROL=2,HOTKEYF_ALT=4,HOTKEYF_SHIFT=1,HOTKEYF_EXT=8,MAPVK_VK_TO_VSC=0;
constexpr UINT DT_SINGLELINE=1,DT_VCENTER=2,DT_LEFT=4,DT_CENTER=8;
struct RECT{int left,top,right,bottom;};struct DRAWITEMSTRUCT{UINT CtlType;HWND hwndItem;HDC hDC;RECT rcItem;UINT itemState;};
struct PAINTSTRUCT{};
inline HDC BeginPaint(HWND,PAINTSTRUCT*){return (HDC)1;}inline void EndPaint(HWND,PAINTSTRUCT*){}
inline unsigned MapVirtualKeyW(unsigned vk,unsigned){return vk;}inline int GetKeyNameTextW(LONG scan,wchar_t*s,int){s[0]=wchar_t((scan>>16)&255);s[1]=0;return 1;}
struct Win {std::wstring cls=L"BUTTON",text=L"text";LONG_PTR style=0;HWND parent=nullptr;bool enabled=true;unsigned hotkey=0;int check=0,invalid=0;bool erased=false;std::map<std::wstring,HANDLE> props;std::map<UINT_PTR,std::pair<Proc,DWORD_PTR>> subs;};
inline HWND focus=nullptr;inline int painted=0,nativePaint=0,cleared=0,parentChecked=-1,mode=0;inline COLORREF fg=0,bg=0;inline int dcSaved=0,dcRestored=0;
inline HBRUSH CreateSolidBrush(COLORREF){return (HBRUSH)1;}inline HGDIOBJ SelectObject(HDC,HGDIOBJ){return (HGDIOBJ)2;}inline HGDIOBJ GetStockObject(int){return (HGDIOBJ)3;}
inline bool RoundRect(HDC,int,int,int,int,int,int){return true;}inline bool DeleteObject(HGDIOBJ){return true;}
inline int FillRect(HDC,RECT*,HBRUSH){++cleared;return 1;}inline int SaveDC(HDC){return ++dcSaved;}inline bool RestoreDC(HDC,int){++dcRestored;return true;}
inline HWND GetFocus(){return focus;}inline BOOL IsWindowEnabled(HWND w){return w->enabled;}inline LONG_PTR GetWindowLongPtrW(HWND w,int){return w->style;}
inline LONG_PTR SetWindowLongPtrW(HWND w,int,LONG_PTR s){auto old=w->style;w->style=s;return old;}
inline HANDLE GetPropW(HWND w,const wchar_t*k){return w->props[k];}inline BOOL SetPropW(HWND w,const wchar_t*k,HANDLE v){w->props[k]=v;return true;}inline HANDLE RemovePropW(HWND w,const wchar_t*k){auto v=w->props[k];w->props.erase(k);return v;}
inline int GetWindowTextW(HWND w,wchar_t*s,int n){std::wcsncpy(s,w->text.c_str(),n);return w->text.size();}
inline int GetClassNameW(HWND w,wchar_t*s,int n){std::wcsncpy(s,w->cls.c_str(),n);return w->cls.size();}
inline int SetBkMode(HDC,int m){mode=m;return 1;}inline COLORREF SetTextColor(HDC,COLORREF c){fg=c;return 0;}inline COLORREF SetBkColor(HDC,COLORREF c){bg=c;return 0;}inline COLORREF SetDCBrushColor(HDC,COLORREF c){return c;}
inline COLORREF GetSysColor(int n){return n==COLOR_WINDOW?RGB(255,255,255):n==COLOR_WINDOWTEXT?0:RGB(128,128,128);}inline HBRUSH GetSysColorBrush(int){return (HBRUSH)4;}
inline std::wstring drawn;inline int DrawTextW(HDC,const wchar_t*t,int,RECT*,UINT){drawn=t;++painted;return 1;}inline void InflateRect(RECT*,int,int){}inline void DrawFocusRect(HDC,RECT*){}
inline void GetClientRect(HWND,RECT*r){*r={0,0,120,28};}inline BOOL InvalidateRect(HWND w,void*,BOOL erase){++w->invalid;w->erased=erase;return true;}
inline BOOL SetWindowSubclass(HWND w,Proc p,UINT_PTR id,DWORD_PTR data){w->subs[id]={p,data};return true;}
inline BOOL GetWindowSubclass(HWND w,Proc p,UINT_PTR id,DWORD_PTR*out){auto i=w->subs.find(id);if(i==w->subs.end()||i->second.first!=p)return false;*out=i->second.second;return true;}
inline BOOL RemoveWindowSubclass(HWND w,Proc,UINT_PTR id){w->subs.erase(id);return true;}
inline HWND GetParent(HWND w){return w->parent;}inline BOOL PostMessageW(HWND,UINT,WPARAM,LPARAM){return true;}
inline int fontHeight=0,fontWeight=0;inline HFONT CreateFontW(int h,int,int,int,int weight,int,int,int,int,int,int,int,int,const wchar_t*){fontHeight=h;fontWeight=weight;return (HFONT)1;}
inline BOOL EnumChildWindows(HWND,BOOL(*)(HWND,LPARAM),LPARAM){return true;}
LRESULT DefSubclassProc(HWND,UINT,WPARAM,LPARAM);LRESULT SendMessageW(HWND,UINT,WPARAM,LPARAM);

#define WINAPI
using HMODULE=void*;
inline HMODULE GetModuleHandleW(const wchar_t*){return nullptr;}inline void*GetProcAddress(HMODULE,const char*){return nullptr;}
inline int MulDiv(int x,int n,int d){return (x*n+d/2)/d;}
constexpr int WS_EX_TOPMOST=8,WS_POPUP=16,TTS_ALWAYSTIP=1,TTS_NOPREFIX=2,CW_USEDEFAULT=0,TTF_IDISHWND=1,TTF_SUBCLASS=2,TTM_ADDTOOLW=400,TTM_UPDATETIPTEXTW=401,TTM_SETMAXTIPWIDTH=402,TTM_SETDELAYTIME=403,TTDT_INITIAL=1,TTDT_AUTOPOP=2,WM_SYSCHAR=310,EN_CHANGE=1,VK_SHIFT=16,VK_CONTROL=17,VK_MENU=18,WM_LBUTTONDOWN=311,WM_RBUTTONUP=312;
constexpr auto TOOLTIPS_CLASSW=L"tooltips_class32";
struct TOOLINFOW{unsigned cbSize,uFlags=0;HWND hwnd=nullptr;UINT_PTR uId=0;wchar_t*lpszText=nullptr;RECT rect{};};
template<class...T>inline HWND CreateWindowExW(T...){return new Win;}inline bool DestroyWindow(HWND w){delete w;return true;}
inline unsigned LOWORD(uintptr_t n){return n&65535;}inline WPARAM MAKEWPARAM(unsigned a,unsigned b){return a|(b<<16);}inline int GetDlgCtrlID(HWND){return 1;}

using HMENU=void*;
struct POINT{int x=0,y=0;};
constexpr auto UPDOWN_CLASSW=L"msctls_updown32",TRACKBAR_CLASSW=L"msctls_trackbar32";
constexpr int WS_VSCROLL=0x200000,WHEEL_DELTA=120;
constexpr UINT WM_MOUSEWHEEL=320,WM_LBUTTONUP=321,WM_LBUTTONDBLCLK=322,WM_MBUTTONDOWN=323,WM_XBUTTONDOWN=324,WM_CONTEXTMENU=325,WM_SHOWWINDOW=326,WM_WINDOWPOSCHANGED=327,BM_CLICK=206;
constexpr UINT TTM_DELTOOLW=404,TTM_NEWTOOLRECTW=405,MF_STRING=0,MF_CHECKED=8,MF_SEPARATOR=0x800,TPM_RETURNCMD=0x100,TPM_NONOTIFY=0x80,TPM_RIGHTBUTTON=2,DT_END_ELLIPSIS=16;
constexpr unsigned VK_SPACE=32,VK_APPS=93,VK_F10=121,XBUTTON1=1;
struct COMBOBOXINFO{unsigned cbSize;HWND hwndCombo=nullptr,hwndItem=nullptr,hwndList=nullptr;};
inline bool IsWindow(HWND w){return w!=nullptr;}inline bool IsWindowVisible(HWND w){return w!=nullptr;}
inline bool GetWindowRect(HWND,RECT*r){*r={0,0,120,28};return true;}
inline int MapWindowPoints(HWND,HWND,POINT*,unsigned){return 0;}
inline bool GetComboBoxInfo(HWND,COMBOBOXINFO*){return false;}
inline short inputState[256]{};inline short GetKeyState(unsigned key){return key<256?inputState[key]:0;}
inline HWND SetFocus(HWND w){auto old=focus;focus=w;return old;}
struct MenuItem{UINT flags,id;std::wstring label;};struct Menu{std::vector<MenuItem> items;};
inline std::vector<MenuItem> lastMenu;inline UINT menuSelection=0;
inline HMENU CreatePopupMenu(){return new Menu;}
inline bool AppendMenuW(HMENU menu,UINT flags,UINT_PTR id,const wchar_t*label){static_cast<Menu*>(menu)->items.push_back({flags,UINT(id),label?label:L""});return true;}
inline UINT TrackPopupMenu(HMENU,UINT,int,int,int,HWND,void*){return menuSelection;}
inline bool DestroyMenu(HMENU menu){lastMenu=static_cast<Menu*>(menu)->items;delete static_cast<Menu*>(menu);return true;}
'''
body=r'''
#include "modern_ui.h"
LRESULT DefSubclassProc(HWND w,UINT m,WPARAM a,LPARAM b){
 if(w->cls==L"PARENT"){
  if(m==WM_COMMAND&&b)parentChecked=SendMessageW((HWND)b,BM_GETCHECK,0,0);
  return 99;
 }
 if(m==HKM_GETHOTKEY)return w->hotkey;if(m==HKM_SETHOTKEY){w->hotkey=a;return 0;}
 if(m==BM_GETCHECK)return w->check;if(m==BM_SETCHECK){w->check=a;return 0;}
 if(m==WM_GETFONT)return 0;if(m==BM_GETSTATE)return 0;if(m==WM_GETDLGCODE)return 0;
 if(m==BM_SETSTYLE){w->style=(w->style&~BS_TYPEMASK)|(a&BS_TYPEMASK);return 0;}
 if(m==WM_SETTEXT)w->text=(const wchar_t*)b;
 if(m==WM_PAINT||m==WM_SETTEXT||m==WM_SETFOCUS||m==BM_SETSTATE){
  if((w->style&BS_TYPEMASK)==BS_OWNERDRAW){DRAWITEMSTRUCT d{ODT_BUTTON,w,(HDC)1,{0,0,120,28},0};modernui::parentProc(w->parent,WM_DRAWITEM,0,(LPARAM)&d,modernui::ParentSubclass,0);}
  else ++nativePaint;
 }
 return 0;
}
LRESULT SendMessageW(HWND w,UINT m,WPARAM a,LPARAM b){DWORD_PTR type=0;if(GetWindowSubclass(w,modernui::hotkeyProc,modernui::HotkeySubclass,&type))return modernui::hotkeyProc(w,m,a,b,modernui::HotkeySubclass,type);if(GetWindowSubclass(w,modernui::buttonProc,modernui::ButtonSubclass,&type))return modernui::buttonProc(w,m,a,b,modernui::ButtonSubclass,type);return DefSubclassProc(w,m,a,b);}
int main(){
 modernui::makeFont();assert(fontHeight==-14&&fontWeight==400);
 modernui::makeFont(modernui::HeadingPixels,96,600);assert(fontHeight==-20&&fontWeight==600);
 for(unsigned dpi:{96u,120u,144u,192u}){
  modernui::makeFont(modernui::BodyPixels,modernui::panelDpi(dpi,true));assert(fontHeight==-int((14*dpi+48)/96));
  modernui::makeFont(modernui::BodyPixels,modernui::panelDpi(dpi,false));assert(fontHeight==-int((14*dpi+48)/96));
 }

 Win parent;parent.cls=L"PARENT";Win toggle;toggle.parent=&parent;toggle.style=BS_AUTOCHECKBOX|WS_TABSTOP;toggle.check=BST_CHECKED;
 modernui::style(&toggle);modernui::style(&toggle);assert((toggle.style&BS_TYPEMASK)==BS_OWNERDRAW&&SendMessageW(&toggle,BM_GETCHECK,0,0)==BST_CHECKED);
 modernui::parentProc(&parent,WM_COMMAND,1,(LPARAM)&toggle,0,0);assert(parentChecked==BST_UNCHECKED);
 modernui::parentProc(&parent,WM_COMMAND,1,(LPARAM)&toggle,0,0);assert(parentChecked==BST_CHECKED);
 Win manual;manual.parent=&parent;manual.style=BS_CHECKBOX;modernui::style(&manual);modernui::parentProc(&parent,WM_COMMAND,1,(LPARAM)&manual,0,0);assert(parentChecked==BST_UNCHECKED);
 for(UINT m:{WM_PAINT,WM_SETTEXT,WM_SETFOCUS,BM_SETSTATE,WM_PRINTCLIENT})SendMessageW(&toggle,m,(WPARAM)(HDC)1,m==WM_SETTEXT?(LPARAM)L"short":0);
 assert(nativePaint==0&&painted==5&&cleared==5&&dcSaved==5&&dcRestored==5);
 Win push;push.parent=&parent;modernui::style(&push);SendMessageW(&push,BM_SETSTYLE,BS_DEFPUSHBUTTON,1);assert((push.style&BS_TYPEMASK)==BS_OWNERDRAW);assert(SendMessageW(&push,WM_GETDLGCODE,0,0)&DLGC_DEFPUSHBUTTON);
 Win specialized;specialized.parent=&parent;specialized.style=BS_OWNERDRAW;modernui::style(&specialized);DWORD_PTR type=0;assert(!GetWindowSubclass(&specialized,modernui::buttonProc,modernui::ButtonSubclass,&type));
 for(const wchar_t*cls:{L"EDIT",L"LISTBOX",L"COMBOBOX",HOTKEY_CLASSW}){Win field;field.cls=cls;for(UINT m:{WM_CTLCOLOREDIT,WM_CTLCOLORLISTBOX,WM_CTLCOLORSTATIC}){assert(modernui::parentProc(&parent,m,(WPARAM)(HDC)1,(LPARAM)&field,0,0)!=99);assert(mode==OPAQUE&&fg!=bg);}}
 Win hotkey;hotkey.cls=HOTKEY_CLASSW;hotkey.parent=&parent;hotkey.hotkey='R'|((HOTKEYF_CONTROL|HOTKEYF_ALT)<<8);
 modernui::style(&hotkey);SendMessageW(&hotkey,WM_PAINT,0,0);assert(focus==nullptr&&drawn==L"按住 Ctrl+Alt → R");
 int n=hotkey.invalid;SendMessageW(&hotkey,HKM_SETHOTKEY,'G'|(HOTKEYF_CONTROL<<8),0);assert(hotkey.invalid==n+1);
 SendMessageW(&hotkey,WM_PRINTCLIENT,(WPARAM)(HDC)1,0);assert(drawn==L"按住 Ctrl → G");
 focus=&hotkey;SendMessageW(&hotkey,WM_SETFOCUS,0,0);focus=nullptr;SendMessageW(&hotkey,WM_KILLFOCUS,0,0);
 int eraseCount=cleared;SendMessageW(&hotkey,HKM_SETHOTKEY,0,0);hotkey.enabled=false;SendMessageW(&hotkey,WM_PAINT,0,0);assert(drawn==L"未设置"&&fg==modernui::Muted&&cleared==eraseCount+1);
 for(UINT m:{WM_KEYDOWN,WM_KEYUP,WM_SYSKEYDOWN,WM_SYSKEYUP,WM_CHAR}){int old=hotkey.invalid;SendMessageW(&hotkey,m,0,0);assert(hotkey.invalid==old);}
 for(UINT m:{WM_ENABLE,WM_SETFONT,WM_SIZE}){int old=hotkey.invalid;SendMessageW(&hotkey,m,0,0);assert(hotkey.invalid==old+1);}
 modernui::mouseOnly(&hotkey);SendMessageW(&hotkey,WM_PAINT,0,0);assert(drawn==L"仅鼠标");
 hotkey.enabled=true;
 for(unsigned replacement:{unsigned('J')|unsigned(keygesture::Mode::Release),0u}){
  SendMessageW(&hotkey,HKM_SETHOTKEY,0,0);SendMessageW(&hotkey,WM_KEYDOWN,VK_CONTROL,0);
  assert(GetPropW(&hotkey,L"RA2.Modern.PendingModifier"));
  SendMessageW(&hotkey,HKM_SETHOTKEY,replacement,0);SendMessageW(&hotkey,WM_KEYUP,VK_CONTROL,0);
  assert(SendMessageW(&hotkey,HKM_GETHOTKEY,0,0)==replacement&&!GetPropW(&hotkey,L"RA2.Modern.PendingModifier"));
 }
 modernui::keyboardOnly(&hotkey);SendMessageW(&hotkey,HKM_SETHOTKEY,'G',0);
 assert(!modernui::hotkeyCommand(&hotkey,modernui::KeyMouseLeft)&&SendMessageW(&hotkey,HKM_GETHOTKEY,0,0)=='G');
 modernui::showHotkeyMenu(&hotkey);for(const auto&item:lastMenu)assert(item.id<modernui::KeyMouseLeft||item.id>modernui::KeyMouseX2);
 Win held;held.cls=HOTKEY_CLASSW;held.parent=&parent;modernui::style(&held);modernui::holdKey(&held);SendMessageW(&held,HKM_SETHOTKEY,'G',0);
 assert(!modernui::hotkeyCommand(&held,modernui::KeyRelease));assert(modernui::hotkeyCommand(&held,modernui::KeyDoubleHold));
 assert(modernui::hotkeyDescription(&held).find(L"按住")!=std::wstring::npos);
 assert(dcSaved==dcRestored);SendMessageW(&hotkey,WM_NCDESTROY,0,0);assert(!GetWindowSubclass(&hotkey,modernui::hotkeyProc,modernui::HotkeySubclass,&type));
 Win label;label.cls=L"STATIC";modernui::staticProc(&label,WM_SETTEXT,0,(LPARAM)L"x",0,0);assert(label.erased&&label.invalid==1);
 SendMessageW(&toggle,WM_NCDESTROY,0,0);assert(!GetPropW(&toggle,L"RA2.Modern.Checked"));
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'windows.h').write_text(header);(p/'commctrl.h').write_text('#pragma once\n');(p/'main.cpp').write_text(body)
 subprocess.run(['g++','-std=c++17','-I'+folder,'-I'+str(root/'shared/source'),str(p/'main.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS production UI owner-draw routing: state/text/focus/print, checkbox notification ordering, default button, native specialized preview, field contrast, full static invalidation; no Windows raster claim')

# Real production display-clock function must progress between coarse OS ticks,
# including with no injected Shared mapping. This is a mocked clock, not FPS.
clock_body=r'''
#include "windows.h"
struct LARGE_INTEGER{long long QuadPart=0;};
static uint32_t coarse=1000;static long long qpc=1000000;static bool qpcOK=true;
uint32_t GetTickCount(){return coarse;}
bool QueryPerformanceFrequency(LARGE_INTEGER*p){p->QuadPart=1000000;return true;}
bool QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=qpc;return qpcOK;}
#define _WIN32
#include "clock_ms.h"
int main(){
 assert(assist::sampleRenderClock(nullptr)==1000);
 qpc+=2000;assert(assist::sampleRenderClock(nullptr)==1002&&assist::sampleClock(nullptr)==1000);
 qpc+=2000;assert(assist::sampleRenderClock(nullptr)==1004);
 qpcOK=false;coarse=1015;assert(assist::sampleRenderClock(nullptr)==1015);
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'windows.h').write_text(header);(p/'clock.cpp').write_text(clock_body)
 subprocess.run(['g++','-std=c++17','-I'+folder,'-I'+str(root/'assist/source'),str(p/'clock.cpp'),'-o',str(p/'clock')],check=True)
 subprocess.run([str(p/'clock')],check=True)
print('PASS production display QPC independent of Shared and command clock, 2 ms progression and timer failure fallback')

for name in ("assist", "unit_monitor", "suite"):
 assert "font=modernui::makeFont()" in (root/name/"source/main.cpp").read_text()
cursor=(root/"cursor/source/main.cpp").read_text()
assert "return modernui::panelDpi(dpi,true)" in cursor and "next=modernui::panelDpi(next,true)" in cursor
assert "HFONT fresh=modernui::makeFont(modernui::BodyPixels,uiDpi)" in cursor
print("PASS shared typography: suite and three panels use one font factory; embedded panels follow the console DPI")
