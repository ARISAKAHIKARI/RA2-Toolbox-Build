
// Executes the real helper with a portable Win32 control-protocol seam.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <map>
#include <string>
using UINT=unsigned;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LRESULT=intptr_t;using UINT_PTR=uintptr_t;using DWORD_PTR=uintptr_t;using INT_PTR=intptr_t;using HANDLE=void*;using HDC=void*;using HFONT=void*;
#define CALLBACK
struct Window;using HWND=Window*;using Subclass=LRESULT(*)(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
struct Window{HWND owner=nullptr;int selected=-1,count=4,commands=0;bool enabled=true;Subclass subclass=nullptr;std::map<std::wstring,HANDLE>properties;};
struct RECT{int left=0,top=0,right=100,bottom=20;};struct TEXTMETRICW{int tmHeight=12;};struct DRAWITEMSTRUCT{UINT CtlType=0,itemID=0,itemState=0;HWND hwndItem=nullptr;HDC hDC=nullptr;RECT rcItem;};
constexpr int TRUE=1,CB_ERR=-1,WM_DRAWITEM=1,WM_COMMAND=2,WM_NCDESTROY=3,CBN_SELCHANGE=4,CB_GETCURSEL=5,CB_SETCURSEL=6,CB_RESETCONTENT=7,CB_GETLBTEXTLEN=8,CB_GETLBTEXT=9,WM_GETFONT=10,CB_SETITEMHEIGHT=11,ODT_COMBOBOX=12,ODS_SELECTED=1,ODS_FOCUS=2,COLOR_HIGHLIGHT=13,COLOR_WINDOW=14,COLOR_HIGHLIGHTTEXT=15,COLOR_WINDOWTEXT=16,COLOR_GRAYTEXT=17,TRANSPARENT=18,DT_SINGLELINE=1,DT_VCENTER=2,DT_NOPREFIX=4;
static unsigned HIWORD(WPARAM w){return unsigned((w>>16)&65535);}static int textColor=0,rollback=-1;
static LRESULT SendMessageW(HWND,UINT,WPARAM,LPARAM);
static HANDLE GetPropW(HWND w,const wchar_t*name){auto i=w->properties.find(name);return i==w->properties.end()?nullptr:i->second;}
static int SetPropW(HWND w,const wchar_t*name,HANDLE v){w->properties[name]=v;return TRUE;}
static HANDLE RemovePropW(HWND w,const wchar_t*name){auto v=GetPropW(w,name);w->properties.erase(name);return v;}
static int SetWindowSubclass(HWND w,Subclass fn,UINT_PTR,DWORD_PTR){w->subclass=fn;return TRUE;}
static int RemoveWindowSubclass(HWND w,Subclass fn,UINT_PTR){if(w->subclass==fn)w->subclass=nullptr;return TRUE;}
static LRESULT DefSubclassProc(HWND w,UINT m,WPARAM a,LPARAM b){switch(m){case CB_GETCURSEL:return w->selected;case CB_SETCURSEL:w->selected=a<uintptr_t(w->count)?int(a):-1;return w->selected;case CB_RESETCONTENT:w->count=0;w->selected=-1;return 0;case CB_GETLBTEXTLEN:return 1;case CB_GETLBTEXT:wcscpy(reinterpret_cast<wchar_t*>(b),L"x");return 1;case WM_COMMAND:++w->commands;if(rollback>=0)SendMessageW(reinterpret_cast<HWND>(b),CB_SETCURSEL,rollback,0);return 0;default:return 0;}}
static LRESULT SendMessageW(HWND w,UINT m,WPARAM a,LPARAM b){return w->subclass?w->subclass(w,m,a,b,0,0):DefSubclassProc(w,m,a,b);}
static HWND GetParent(HWND w){return w->owner;}static bool IsWindowEnabled(HWND w){return w->enabled;}
static HDC GetDC(HWND){return nullptr;}static void*SelectObject(HDC,void*p){return p;}static int GetTextMetricsW(HDC,TEXTMETRICW*){return TRUE;}static void ReleaseDC(HWND,HDC){}static void InvalidateRect(HWND,void*,int){}
static int GetSysColorBrush(int c){return c;}static void FillRect(HDC,RECT*,int){}static void SetBkMode(HDC,int){}static int GetSysColor(int c){return c;}static void SetTextColor(HDC,int c){textColor=c;}static void DrawTextW(HDC,const wchar_t*,int,RECT*,int){}static void DrawFocusRect(HDC,RECT*){}
#include "disabled_choices.h"
int main(){unsigned checks=0;auto check=[&](bool v){assert(v);++checks;};Window parent,combo;combo.owner=&parent;
 SendMessageW(&combo,CB_SETCURSEL,1,0);disabledchoices::set(&combo,0x0b);
 auto notify=[&]{SendMessageW(&parent,WM_COMMAND,WPARAM(CBN_SELCHANGE)<<16,reinterpret_cast<LPARAM>(&combo));};
 auto choose=[&](int row){combo.selected=row;notify();}; // Native selection updates precede CBN_SELCHANGE; no programmatic CB_SETCURSEL occurs.
 choose(2);check(combo.selected==1&&parent.commands==0);
 choose(3);check(combo.selected==3&&parent.commands==1);
 choose(2);check(combo.selected==3&&parent.commands==1);
 choose(-1);check(combo.selected==3&&parent.commands==1);
 SendMessageW(&combo,CB_SETCURSEL,0,0);choose(2);check(combo.selected==0&&parent.commands==1);
 SendMessageW(&combo,CB_SETCURSEL,99,0);notify();check(combo.selected==0&&parent.commands==1);
 rollback=0;choose(1);check(combo.selected==0&&parent.commands==2);rollback=-1;choose(2);check(combo.selected==0);
 SendMessageW(&combo,CB_SETCURSEL,2,0);disabledchoices::set(&combo,0x0b);choose(-1);check(combo.selected==2); // Explicit legacy state is retained, never silently migrated.
 choose(1);choose(2);check(combo.selected==1&&parent.commands==3);
 DRAWITEMSTRUCT draw;draw.CtlType=ODT_COMBOBOX;draw.hwndItem=&combo;draw.itemID=2;SendMessageW(&parent,WM_DRAWITEM,0,reinterpret_cast<LPARAM>(&draw));check(textColor==COLOR_GRAYTEXT);
 draw.itemID=1;SendMessageW(&parent,WM_DRAWITEM,0,reinterpret_cast<LPARAM>(&draw));check(textColor==COLOR_WINDOWTEXT);
 combo.enabled=false;SendMessageW(&parent,WM_DRAWITEM,0,reinterpret_cast<LPARAM>(&draw));check(textColor==COLOR_GRAYTEXT);
 SendMessageW(&combo,CB_RESETCONTENT,0,0);choose(-1);check(combo.selected==-1);
 SendMessageW(&combo,WM_NCDESTROY,0,0);check(combo.properties.empty()&&!combo.subclass);
 SendMessageW(&parent,WM_NCDESTROY,0,0);check(!parent.subclass);
 std::printf("production disabled_choices helper: %u checks passed (portable Win32 seam; not a Windows UI run)\n",checks);
}
