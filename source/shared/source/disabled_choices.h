#pragma once
#include <windows.h>
#include <commctrl.h>
namespace disabledchoices {
inline constexpr wchar_t Property[]=L"RA2AvailableChoiceMask";
inline constexpr wchar_t SelectionProperty[]=L"RA2AvailableChoiceSelection";
inline void remember(HWND combo){
 const int row=int(SendMessageW(combo,CB_GETCURSEL,0,0));
 SetPropW(combo,SelectionProperty,reinterpret_cast<HANDLE>(INT_PTR(row+1)));
}
inline LRESULT CALLBACK choice(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR){
 const auto result=DefSubclassProc(w,m,a,b);
 if((m==CB_SETCURSEL&&result!=CB_ERR)||m==CB_RESETCONTENT)remember(w);
 if(m==WM_NCDESTROY){RemovePropW(w,Property);RemovePropW(w,SelectionProperty);RemoveWindowSubclass(w,choice,0x1382);}
 return result;
}
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
  if(mask&&(row<0||row>=32||!((mask>>row)&1))){
   // A disabled click must preserve the previous value, including a legacy
   // disabled selection loaded at startup. Never silently select mode zero.
   const int previous=int(reinterpret_cast<INT_PTR>(GetPropW(c,SelectionProperty)))-1;
   SendMessageW(c,CB_SETCURSEL,previous,0);return 0;
  }
  if(mask){const auto result=DefSubclassProc(w,m,a,b);remember(c);return result;}
 }
 if(m==WM_NCDESTROY)RemoveWindowSubclass(w,parent,0x1381);
 return DefSubclassProc(w,m,a,b);
}
inline void set(HWND combo,uintptr_t allowedMask){
 SetPropW(combo,Property,(HANDLE)allowedMask);remember(combo);SetWindowSubclass(combo,choice,0x1382,0);SetWindowSubclass(GetParent(combo),parent,0x1381,0);
 HDC dc=GetDC(combo);auto old=SelectObject(dc,(HFONT)SendMessageW(combo,WM_GETFONT,0,0));TEXTMETRICW tm{};GetTextMetricsW(dc,&tm);SelectObject(dc,old);ReleaseDC(combo,dc);
 SendMessageW(combo,CB_SETITEMHEIGHT,0,tm.tmHeight+6);SendMessageW(combo,CB_SETITEMHEIGHT,WPARAM(-1),tm.tmHeight+6);
 InvalidateRect(combo,nullptr,TRUE);
}
}
