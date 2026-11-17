#pragma once
#include <stdint.h>
// Stateless, branch-only scheduling filter for WH_GETMESSAGE/POST.
// No game pointers, locks, allocations, Win32 calls or exception changes.
// Input list exactly mirrors AssistRuntimeFilter's native event admission.
namespace runtimehot {
inline bool filteredInput(unsigned message){
 switch(message){
  case WM_MOUSEMOVE:
  case WM_KEYDOWN:case WM_KEYUP:case WM_SYSKEYDOWN:case WM_SYSKEYUP:
  case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:case WM_LBUTTONUP:
  case WM_RBUTTONDOWN:case WM_RBUTTONDBLCLK:case WM_RBUTTONUP:
  case WM_MBUTTONDOWN:case WM_MBUTTONDBLCLK:case WM_MBUTTONUP:
  case WM_XBUTTONDOWN:case WM_XBUTTONDBLCLK:case WM_XBUTTONUP:return true;
  default:return false;
 }
}
inline bool isWake(const MSG&msg,unsigned wakeTag){
 return msg.message==WM_NULL&&msg.wParam==wakeTag;
}
inline bool needsGetMessage(int code,WPARAM wp,const MSG*msg,
                            uint32_t ready,uint32_t enabled,
                            unsigned wakeTag,uint32_t tacticalMask){
 if(code<0||wp!=PM_REMOVE||!msg)return false;
 // Never postpone already queued work, even when another client has just
 // toggled enabled: the real dispatcher consumes the atomics consistently.
 if(ready||isWake(*msg,wakeTag))return true;
 return (enabled&tacticalMask)&&filteredInput(msg->message);
}
inline bool needsCursorPost(int code,const CWPRETSTRUCT*event,uint32_t cursorEnabled){
 return code>=0&&cursorEnabled&&event&&event->message==WM_SETCURSOR;
}
}
