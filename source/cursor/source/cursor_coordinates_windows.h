#pragma once
#include <windows.h>
#include "cursor_coordinates.h"

namespace cursorcoordinates {
enum class Domain {CallerLogical,Physical};
struct WindowsApi {
 using SetContext=HANDLE(WINAPI*)(HANDLE);
 using GetContext=HANDLE(WINAPI*)();
 using Awareness=int(WINAPI*)(HANDLE);
 using Equal=BOOL(WINAPI*)(HANDLE,HANDLE);
 using PhysicalPosition=BOOL(WINAPI*)(POINT*);
 SetContext setContext=nullptr;GetContext getContext=nullptr;Awareness awareness=nullptr;Equal equal=nullptr;PhysicalPosition physicalPosition=nullptr;
 WindowsApi(){
  const HMODULE user32=GetModuleHandleW(L"user32.dll");
  if(!user32)return;
  setContext=reinterpret_cast<SetContext>(GetProcAddress(user32,"SetThreadDpiAwarenessContext"));
  getContext=reinterpret_cast<GetContext>(GetProcAddress(user32,"GetThreadDpiAwarenessContext"));
  awareness=reinterpret_cast<Awareness>(GetProcAddress(user32,"GetAwarenessFromDpiAwarenessContext"));
  equal=reinterpret_cast<Equal>(GetProcAddress(user32,"AreDpiAwarenessContextsEqual"));
  physicalPosition=reinterpret_cast<PhysicalPosition>(GetProcAddress(user32,"GetPhysicalCursorPos"));
 }
};
inline const WindowsApi& windowsApi(){static const WindowsApi value;return value;}
// Keep this scope alive through creation or the final window-position/image
// submission. Restoring DPI awareness immediately after reading would mix domains.
class CoordinateScope {
 const WindowsApi& api_;HANDLE restore_=nullptr;
public:
 Domain domain=Domain::CallerLogical;HANDLE originalContext=nullptr,currentContext=nullptr;
 POINT rawCursor{};bool rawValid=false;DWORD error=0;const wchar_t* route=L"caller-logical-fallback";
 explicit CoordinateScope(bool captureRaw=false):api_(windowsApi()){
  if(captureRaw)rawValid=GetCursorPos(&rawCursor)!=FALSE;
  if(api_.getContext)originalContext=api_.getContext();
  if(api_.setContext){
   restore_=api_.setContext(reinterpret_cast<HANDLE>(intptr_t(-4)));
   if(restore_){domain=Domain::Physical;route=L"physical-pmv2";}
   else{
    restore_=api_.setContext(reinterpret_cast<HANDLE>(intptr_t(-3)));
    if(restore_){domain=Domain::Physical;route=L"physical-pmv1";}
    else error=GetLastError();
   }
  }
  if(restore_&&!originalContext)originalContext=restore_;
  currentContext=api_.getContext?api_.getContext():nullptr;
  if(!restore_&&api_.awareness&&currentContext&&api_.awareness(currentContext)==2){domain=Domain::Physical;route=L"physical-existing-context";}
  if(domain==Domain::CallerLogical&&!error)error=ERROR_CALL_NOT_IMPLEMENTED;
 }
 ~CoordinateScope(){if(restore_)api_.setContext(restore_);}
 CoordinateScope(const CoordinateScope&)=delete;CoordinateScope& operator=(const CoordinateScope&)=delete;
 bool compatible(Domain expected,HANDLE expectedContext)const{
  if(domain!=expected)return false;
  if(domain==Domain::Physical)return true;
  if(expectedContext&&currentContext&&api_.equal)return api_.equal(expectedContext,currentContext)!=FALSE;
  return expectedContext==currentContext;
 }
 const wchar_t* cursorApi()const{return domain==Domain::Physical&&api_.physicalPosition?L"GetPhysicalCursorPos":L"GetCursorPos";}
 bool cursor(POINT& point)const{
  POINT value{};
  // Never switch APIs after a failed physical read. That can turn a real
  // unavailable-desktop/input error into an unrelated coordinate sample.
  const bool ok=domain==Domain::Physical&&api_.physicalPosition?api_.physicalPosition(&value)!=FALSE:GetCursorPos(&value)!=FALSE;
  if(ok)point=value;return ok;
 }
 bool client(HWND window,Rect& bounds)const{
  RECT local{};POINT a{},b{};
  if(!GetClientRect(window,&local))return false;
  if(local.right<=local.left||local.bottom<=local.top){SetLastError(ERROR_INVALID_DATA);return false;}
  a={local.left,local.top};b={local.right,local.bottom};
  if(!ClientToScreen(window,&a)||!ClientToScreen(window,&b))return false;
  const Rect value{a.x,a.y,b.x,b.y};if(!value.valid()){SetLastError(ERROR_INVALID_DATA);return false;}
  bounds=value;return true;
 }
};
}
