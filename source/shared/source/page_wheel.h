#pragma once
#include <windows.h>
#include <commctrl.h>
#include <cwchar>
#include <new>

// Wheel input in a settings field belongs to its page, never to the value.
// Posting also keeps a module's scroll/layout work out of another UI thread.
namespace pagewheel {
constexpr UINT_PTR FieldSubclass=0x6250, PageSubclass=0x6251;
constexpr wchar_t StateProperty[]=L"RA2.PageWheel.State";
constexpr wchar_t TargetProperty[]=L"RA2.PageWheel.Target";
struct State {int remainder=0;};

inline bool nativeField(HWND window){
 wchar_t name[64]{};GetClassNameW(window,name,64);
 return !_wcsicmp(name,L"COMBOBOX")||!_wcsicmp(name,L"ComboLBox")||
  !_wcsicmp(name,L"EDIT")||!_wcsicmp(name,L"LISTBOX")||
  !_wcsicmp(name,L"SysListView32")||!_wcsicmp(name,L"SysTreeView32")||
  !_wcsicmp(name,HOTKEY_CLASSW)||!_wcsicmp(name,UPDOWN_CLASSW)||
  !_wcsicmp(name,TRACKBAR_CLASSW);
}
inline HWND page(HWND field){
 if(auto target=static_cast<HWND>(GetPropW(field,TargetProperty));target&&IsWindow(target))return target;
 HWND next=GetParent(field);
 for(unsigned depth=0;next&&depth<32;++depth){
  if((GetWindowLongPtrW(next,GWL_STYLE)&WS_VSCROLL)&&!nativeField(next))return next;
  HWND parent=GetParent(next);if(parent==next)break;next=parent;
 }
 return nullptr;
}
inline LRESULT CALLBACK pageProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(message==WM_NCDESTROY){delete static_cast<State*>(RemovePropW(window,StateProperty));RemoveWindowSubclass(window,pageProc,id);}
 return DefSubclassProc(window,message,wp,lp);
}
inline int delta(HWND window,WPARAM input,int pixelsPerNotch){
 auto state=static_cast<State*>(GetPropW(window,StateProperty));
 if(!state){
  state=new(std::nothrow) State;
  if(state&&(!SetPropW(window,StateProperty,state)||!SetWindowSubclass(window,pageProc,PageSubclass,0))){RemovePropW(window,StateProperty);delete state;state=nullptr;}
 }
 const int amount=static_cast<short>(HIWORD(input));
 if(!state)return amount/WHEEL_DELTA*pixelsPerNotch;
 state->remainder+=amount;const int steps=state->remainder/WHEEL_DELTA;
 state->remainder%=WHEEL_DELTA;return steps*pixelsPerNotch;
}
inline LRESULT CALLBACK fieldProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
 if(message==WM_NCDESTROY){RemovePropW(window,TargetProperty);RemoveWindowSubclass(window,fieldProc,id);}
 if(message==WM_MOUSEWHEEL){
  if(HWND target=page(window))PostMessageW(target,WM_MOUSEWHEEL,wp,lp);
  // Do not pass this to COMBOBOX/UPDOWN/HOTKEY, even at a page boundary.
  return 0;
 }
 return DefSubclassProc(window,message,wp,lp);
}
inline void hook(HWND window,HWND target=nullptr){if(window){if(target)SetPropW(window,TargetProperty,target);SetWindowSubclass(window,fieldProc,FieldSubclass,0);}}
inline void style(HWND window){
 hook(window);
 wchar_t name[32]{};GetClassNameW(window,name,32);
 if(!_wcsicmp(name,L"COMBOBOX")){
  COMBOBOXINFO info{sizeof info};
  if(GetComboBoxInfo(window,&info)){HWND target=page(window);hook(info.hwndItem,target);hook(info.hwndList,target);}
 }
}
}
