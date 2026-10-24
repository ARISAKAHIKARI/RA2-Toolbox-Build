#pragma once
#include <windows.h>
#include <commctrl.h>
namespace disabledchoices {
inline constexpr wchar_t Property[]=L"RA2AvailableChoiceMask";
inline LRESULT CALLBACK parent(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR){
 if(m==WM_DRAWITEM){auto d=reinterpret_cast<DRAWITEMSTRUCT*>(b);
  uintptr_t mask=d?reinterpret_cast<uintptr_t>(GetPropW(d->hwndItem,Property)):0;
  if(d&&d->CtlType==ODT_COMBOBOX&&mask){
   bool available=d->itemID<32&&((mask>>d->itemID)&1)&&IsWindowEnabled(d->hwndItem);
   bool selected=(d->itemState&ODS_SELECTED)!=0;HDC dc=d->hDC;
   FillRect(dc,&d->rcItem,GetSysColorBrush(selected?COLOR_HIGHLIGHT:COLOR_WINDOW));
   SetBkMode(dc,TRANSPARENT);SetTextColor(dc,GetSysColor(available?(selected?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT):COLOR_GRAYTEXT));
   wchar_t text[256]{};if(d->itemID!=UINT(-1)&&SendMessageW(d->hwndItem,CB_GETLBTEXTLEN,d->itemID,0)<256)SendMessageW(d->hwndItem,CB_GETLBTEXT,d->itemID,(LPARAM)text);
   RECT r=d->rcItem;r.left+=6;DrawTextW(dc,text,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
   if(d->itemState&ODS_FOCUS)DrawFocusRect(dc,&d->rcItem);return TRUE;
  }
 }
 if(m==WM_COMMAND&&HIWORD(a)==CBN_SELCHANGE){HWND c=(HWND)b;uintptr_t mask=reinterpret_cast<uintptr_t>(GetPropW(c,Property));
  int row=int(SendMessageW(c,CB_GETCURSEL,0,0));
  if(mask&&(row<0||row>=32||!((mask>>row)&1))){for(int i=0;i<32;++i)if((mask>>i)&1){SendMessageW(c,CB_SETCURSEL,i,0);break;}return 0;}
 }
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,parent,0x1381);
 return DefSubclassProc(w,m,a,b);
}
inline void set(HWND combo,uintptr_t allowedMask){
 SetPropW(combo,Property,(HANDLE)allowedMask);SetWindowSubclass(GetParent(combo),parent,0x1381,0);
 HDC dc=GetDC(combo);auto old=SelectObject(dc,(HFONT)SendMessageW(combo,WM_GETFONT,0,0));TEXTMETRICW tm{};GetTextMetricsW(dc,&tm);SelectObject(dc,old);ReleaseDC(combo,dc);
 SendMessageW(combo,CB_SETITEMHEIGHT,0,tm.tmHeight+6);SendMessageW(combo,CB_SETITEMHEIGHT,WPARAM(-1),tm.tmHeight+6);
 InvalidateRect(combo,nullptr,TRUE);
}
}
