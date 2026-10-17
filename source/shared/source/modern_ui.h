#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <cwchar>
#include <string>
namespace modernui {
constexpr int BodyPixels=14,HeadingPixels=20;
// Embedded panels share the suite's physical-pixel layout. Standalone cursor
// windows retain their existing DPI-scaled layout; do not scale one pane alone.
constexpr unsigned panelDpi(unsigned dpi,bool embedded){return embedded?96:(dpi>=48&&dpi<=768?dpi:96);}
inline HFONT makeFont(int pixels=BodyPixels,unsigned dpi=96,int weight=FW_NORMAL){
 return CreateFontW(-int((pixels*panelDpi(dpi,false)+48)/96),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
}
constexpr UINT FocusMessage=WM_APP+94;
constexpr COLORREF Background=RGB(18,24,34),Surface=RGB(26,35,49),Field=RGB(32,43,59),Text=RGB(231,238,247),Muted=RGB(153,170,193),Accent=RGB(104,198,255);
inline void rounded(HDC dc,RECT r,COLORREF color,int radius=10){HBRUSH b=CreateSolidBrush(color);auto old=SelectObject(dc,b);auto pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,pen);SelectObject(dc,old);DeleteObject(b);}
constexpr UINT_PTR ButtonSubclass=0x6215,ParentSubclass=0x6217,StaticSubclass=0x6218;
inline bool toggleType(DWORD_PTR type){return type==BS_AUTOCHECKBOX||type==BS_CHECKBOX;}
inline void paintButton(HWND w,HDC dc,RECT r,DWORD_PTR type,UINT state){
 int saved=SaveDC(dc);bool enabled=!(state&ODS_DISABLED),focus=(state&ODS_FOCUS)!=0;
 bool toggle=toggleType(type),checked=GetPropW(w,L"RA2.Modern.Checked")!=nullptr;
 bool pressed=(state&ODS_SELECTED)!=0,selected=GetPropW(w,L"RA2.Modern.Selected")!=nullptr;
 COLORREF fill=toggle?Background:selected?RGB(36,67,91):pressed?RGB(46,66,87):Surface;
 HBRUSH base=CreateSolidBrush(Background);FillRect(dc,&r,base);DeleteObject(base);
 if(!toggle)rounded(dc,r,fill);
 RECT text=r;text.left+=toggle?44:12;text.right-=8;
 if(toggle){int y=r.top+(r.bottom-r.top-18)/2;RECT rail{r.left+2,y,r.left+34,y+18};rounded(dc,rail,enabled&&checked?Accent:RGB(67,80,98),18);RECT dot{r.left+(checked?18:4),y+2,r.left+(checked?32:18),y+16};rounded(dc,dot,checked?Background:Text,14);}
 wchar_t label[512]{};GetWindowTextW(w,label,512);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,!enabled?RGB(106,121,141):selected?Accent:Text);
 auto font=(HFONT)SendMessageW(w,WM_GETFONT,0,0);if(font)SelectObject(dc,font);
 DrawTextW(dc,label,-1,&text,DT_SINGLELINE|DT_VCENTER|(toggle||selected?DT_LEFT:DT_CENTER));
 if(focus&&!(state&ODS_NOFOCUSRECT)){RECT f=r;InflateRect(&f,-3,-3);DrawFocusRect(dc,&f);}
 if(saved)RestoreDC(dc,saved);
}
inline LRESULT CALLBACK buttonProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR type){
 if(m==WM_NCDESTROY){RemovePropW(w,L"RA2.Modern.Checked");RemovePropW(w,L"RA2.Modern.Selected");RemoveWindowSubclass(w,buttonProc,id);return DefSubclassProc(w,m,wp,lp);}
 if(m==BM_GETCHECK&&toggleType(type))return GetPropW(w,L"RA2.Modern.Checked")?BST_CHECKED:BST_UNCHECKED;
 if(m==BM_SETCHECK&&toggleType(type)){if(wp==BST_CHECKED)SetPropW(w,L"RA2.Modern.Checked",(HANDLE)1);else RemovePropW(w,L"RA2.Modern.Checked");InvalidateRect(w,nullptr,FALSE);return 0;}
 // Keep dialog-manager default-button updates from restoring native painting.
 if(m==BM_SETSTYLE){type=wp&BS_TYPEMASK;if(type==BS_PUSHBUTTON||type==BS_DEFPUSHBUTTON)SetWindowSubclass(w,buttonProc,id,type);wp=(wp&~WPARAM(BS_TYPEMASK))|BS_OWNERDRAW;}
 if(m==WM_GETDLGCODE){auto result=DefSubclassProc(w,m,wp,lp);if(!toggleType(type))return (result&~(DLGC_DEFPUSHBUTTON|DLGC_UNDEFPUSHBUTTON))|(type==BS_DEFPUSHBUTTON?DLGC_DEFPUSHBUTTON:DLGC_UNDEFPUSHBUTTON);return result;}
 if(m==WM_PRINTCLIENT){RECT r{};GetClientRect(w,&r);UINT state=IsWindowEnabled(w)?0:ODS_DISABLED;if(GetFocus()==w)state|=ODS_FOCUS;if(SendMessageW(w,BM_GETSTATE,0,0)&BST_PUSHED)state|=ODS_SELECTED;paintButton(w,(HDC)wp,r,type,state);return 0;}
 // BS_OWNERDRAW makes ALL native state changes use the same WM_DRAWITEM renderer.
 // Merely replacing WM_PAINT leaves native WM_SETTEXT/focus/BM_SETSTATE drawing.
 return DefSubclassProc(w,m,wp,lp);
}
// Keep the native hotkey state machine and notifications; own the entire
// painted field so unfocused values never depend on native erase/focus order.
constexpr UINT_PTR HotkeySubclass=0x6219;
inline void paintHotkey(HWND w,HDC dc){
 int saved=SaveDC(dc);RECT r{};GetClientRect(w,&r);
 auto brush=CreateSolidBrush(Field);FillRect(dc,&r,brush);DeleteObject(brush);
 const auto key=static_cast<unsigned>(SendMessageW(w,HKM_GETHOTKEY,0,0));
 std::wstring label;const unsigned vk=key&255,mods=(key>>8)&255;
 if(vk){
  if(mods&HOTKEYF_CONTROL)label+=L"Ctrl + ";
  if(mods&HOTKEYF_ALT)label+=L"Alt + ";
  if(mods&HOTKEYF_SHIFT)label+=L"Shift + ";
  wchar_t name[96]{};
  unsigned scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC)<<16;
  if(mods&HOTKEYF_EXT)scan|=1u<<24;
  if(GetKeyNameTextW(static_cast<LONG>(scan),name,96)>0)label+=name;
  else label+=L"VK "+std::to_wstring(vk);
 }else label=GetPropW(w,L"RA2.Modern.MouseOnly")?L"仅鼠标":L"未设置";
 auto font=(HFONT)SendMessageW(w,WM_GETFONT,0,0);if(font)SelectObject(dc,font);
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,IsWindowEnabled(w)?Text:Muted);
 RECT text=r;text.left+=8;text.right-=8;
 DrawTextW(dc,label.c_str(),-1,&text,DT_SINGLELINE|DT_VCENTER|DT_LEFT);
 if(GetFocus()==w){InflateRect(&r,-2,-2);DrawFocusRect(dc,&r);}
 if(saved)RestoreDC(dc,saved);
}
inline LRESULT CALLBACK hotkeyProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY){RemovePropW(w,L"RA2.Modern.MouseOnly");RemoveWindowSubclass(w,hotkeyProc,id);return DefSubclassProc(w,m,wp,lp);}
 if(m==WM_ERASEBKGND)return TRUE;
 if(m==WM_PAINT){PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);paintHotkey(w,dc);EndPaint(w,&ps);return 0;}
 if(m==WM_PRINTCLIENT){paintHotkey(w,(HDC)wp);return 0;}
 auto result=DefSubclassProc(w,m,wp,lp);
 if(m==HKM_SETHOTKEY||m==WM_KEYDOWN||m==WM_KEYUP||m==WM_SYSKEYDOWN||m==WM_SYSKEYUP||m==WM_CHAR||m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_ENABLE||m==WM_SETFONT||m==WM_SIZE)
  InvalidateRect(w,nullptr,FALSE);
 return result;
}
inline LRESULT CALLBACK parentProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,parentProc,id);
 if(m==WM_DRAWITEM&&lp){auto*d=(DRAWITEMSTRUCT*)lp;DWORD_PTR type=0;
  if(d->CtlType==ODT_BUTTON&&GetWindowSubclass(d->hwndItem,buttonProc,ButtonSubclass,&type)){paintButton(d->hwndItem,d->hDC,d->rcItem,type,d->itemState);return TRUE;}}
 if(m==WM_COMMAND&&lp&&HIWORD(wp)==BN_CLICKED){DWORD_PTR type=0;auto ctl=(HWND)lp;
  if(GetWindowSubclass(ctl,buttonProc,ButtonSubclass,&type)&&type==BS_AUTOCHECKBOX){
   // Native owner-draw buttons emit BN_CLICKED for both mouse and Space/BM_CLICK.
   // Publish the new check value before the existing parent settings handler runs.
   SendMessageW(ctl,BM_SETCHECK,SendMessageW(ctl,BM_GETCHECK,0,0)==BST_CHECKED?BST_UNCHECKED:BST_CHECKED,0);
  }}
 if(m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX||m==WM_CTLCOLORSTATIC){
  wchar_t cls[40]{};GetClassNameW((HWND)lp,cls,40);auto dc=(HDC)wp;
  // HOTKEY_CLASS has no documented custom-background API and may erase with
  // COLOR_WINDOW. Keep its foreground and background a matching system pair.
  if(!_wcsicmp(cls,HOTKEY_CLASSW)){SetBkMode(dc,OPAQUE);SetTextColor(dc,GetSysColor(IsWindowEnabled((HWND)lp)?COLOR_WINDOWTEXT:COLOR_GRAYTEXT));SetBkColor(dc,GetSysColor(COLOR_WINDOW));return (LRESULT)GetSysColorBrush(COLOR_WINDOW);}
  if(!_wcsicmp(cls,L"EDIT")||!_wcsicmp(cls,L"LISTBOX")||!_wcsicmp(cls,L"COMBOBOX")){
   SetBkMode(dc,OPAQUE);SetTextColor(dc,IsWindowEnabled((HWND)lp)?Text:Muted);SetBkColor(dc,Field);SetDCBrushColor(dc,Field);return (LRESULT)GetStockObject(DC_BRUSH);
  }
 }
 return DefSubclassProc(w,m,wp,lp);
}
inline LRESULT CALLBACK staticProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,staticProc,id);
 auto result=DefSubclassProc(w,m,wp,lp);
 if(m==WM_SETTEXT||m==WM_SETFONT||m==WM_ENABLE)InvalidateRect(w,nullptr,TRUE);
 return result;
}
inline void mouseOnly(HWND w){SetPropW(w,L"RA2.Modern.MouseOnly",(HANDLE)1);InvalidateRect(w,nullptr,FALSE);}
inline void heading(HWND w){static HFONT font=makeFont(HeadingPixels,96,FW_SEMIBOLD);SendMessageW(w,WM_SETFONT,(WPARAM)font,TRUE);}
inline LRESULT CALLBACK focusProc(HWND w,UINT m,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,focusProc,id);
 if(m==WM_SETFOCUS)PostMessageW(GetParent(w),FocusMessage,0,(LPARAM)w);
 return DefSubclassProc(w,m,wp,lp);
}
inline void style(HWND w){
 wchar_t cls[40]{};GetClassNameW(w,cls,40);
 if(auto parent=GetParent(w))SetWindowSubclass(parent,parentProc,ParentSubclass,0);
 if(GetWindowLongPtrW(w,GWL_STYLE)&WS_TABSTOP)SetWindowSubclass(w,focusProc,0x6216,0);
 if(!_wcsicmp(cls,HOTKEY_CLASSW)){SetWindowSubclass(w,hotkeyProc,HotkeySubclass,0);InvalidateRect(w,nullptr,FALSE);}
 if(!_wcsicmp(cls,L"STATIC"))SetWindowSubclass(w,staticProc,StaticSubclass,0);
 if(!_wcsicmp(cls,L"BUTTON")){
  DWORD_PTR prior=0;if(GetWindowSubclass(w,buttonProc,ButtonSubclass,&prior))return;
  auto style=GetWindowLongPtrW(w,GWL_STYLE);auto type=style&BS_TYPEMASK;
  // Existing specialized owner-drawn previews retain their own renderer.
  if(type==BS_PUSHBUTTON||type==BS_DEFPUSHBUTTON||toggleType(type)){
   bool checked=SendMessageW(w,BM_GETCHECK,0,0)==BST_CHECKED;
   if(SetWindowSubclass(w,buttonProc,ButtonSubclass,type)){
    if(checked)SetPropW(w,L"RA2.Modern.Checked",(HANDLE)1);
    SetWindowLongPtrW(w,GWL_STYLE,(style&~LONG_PTR(BS_TYPEMASK))|BS_OWNERDRAW);
    InvalidateRect(w,nullptr,TRUE);
   }
  }
 }
}
inline BOOL CALLBACK styleChild(HWND w,LPARAM){style(w);return TRUE;}
inline void styleChildren(HWND parent){EnumChildWindows(parent,styleChild,0);}
inline void select(HWND w,bool value){if(value)SetPropW(w,L"RA2.Modern.Selected",(HANDLE)1);else RemovePropW(w,L"RA2.Modern.Selected");InvalidateRect(w,nullptr,FALSE);}
}
