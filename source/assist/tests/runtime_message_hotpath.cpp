// Exercise the real production Win32 message admission filter with synthetic
// messages: all existing input gestures and queued work must still run, while
// repaint/mouse hover noise and unrelated cursor-post messages are pure no-ops.
#include <cassert>
#include <cstdint>
#include <cstdio>
using WPARAM=uintptr_t;
struct MSG {unsigned message=0;WPARAM wParam=0;uintptr_t lParam=0;};
struct CWPRETSTRUCT {uintptr_t lResult=0,lParam=0;WPARAM wParam=0;unsigned message=0;};
constexpr unsigned PM_REMOVE=1,PM_NOREMOVE=0;
constexpr unsigned WM_NULL=0x0000,WM_PAINT=0x000F,WM_TIMER=0x0113,WM_SETCURSOR=0x0020;
constexpr unsigned WM_KEYDOWN=0x0100,WM_KEYUP=0x0101,WM_SYSKEYDOWN=0x0104,WM_SYSKEYUP=0x0105;
constexpr unsigned WM_MOUSEMOVE=0x0200;
constexpr unsigned WM_LBUTTONDOWN=0x0201,WM_LBUTTONUP=0x0202,WM_LBUTTONDBLCLK=0x0203;
constexpr unsigned WM_RBUTTONDOWN=0x0204,WM_RBUTTONUP=0x0205,WM_RBUTTONDBLCLK=0x0206;
constexpr unsigned WM_MBUTTONDOWN=0x0207,WM_MBUTTONUP=0x0208,WM_MBUTTONDBLCLK=0x0209;
constexpr unsigned WM_XBUTTONDOWN=0x020B,WM_XBUTTONUP=0x020C,WM_XBUTTONDBLCLK=0x020D;
#include "../../game_runtime/source/runtime_message_hotpath.h"
constexpr unsigned Tactical=1,WakeTag=0x52545736u;
int main(){
 unsigned checked=0,skipped=0;
 for(unsigned msg=0;msg<0x400;++msg){
  MSG m{msg,0,0};
  const bool input=runtimehot::filteredInput(msg);
  const bool wanted=runtimehot::needsGetMessage(0,PM_REMOVE,&m,0,Tactical,WakeTag,Tactical);
  assert(wanted==input);++checked;if(!wanted)++skipped;
  assert(!runtimehot::needsGetMessage(0,PM_NOREMOVE,&m,0,Tactical,WakeTag,Tactical));
  assert(!runtimehot::needsGetMessage(-1,PM_REMOVE,&m,0,Tactical,WakeTag,Tactical));
  assert(!runtimehot::needsGetMessage(0,PM_REMOVE,&m,0,0,WakeTag,Tactical));
  assert(runtimehot::needsGetMessage(0,PM_REMOVE,&m,1,0,WakeTag,Tactical));
  assert(runtimehot::needsGetMessage(0,PM_REMOVE,&m,2,0,WakeTag,Tactical));
  CWPRETSTRUCT post{};post.message=msg;
  assert(runtimehot::needsCursorPost(0,&post,1)==(msg==WM_SETCURSOR));
  assert(!runtimehot::needsCursorPost(0,&post,0));
 }
 constexpr unsigned inputs[]={
  WM_MOUSEMOVE,WM_KEYDOWN,WM_KEYUP,WM_SYSKEYDOWN,WM_SYSKEYUP,
  WM_LBUTTONDOWN,WM_LBUTTONDBLCLK,WM_LBUTTONUP,
  WM_RBUTTONDOWN,WM_RBUTTONDBLCLK,WM_RBUTTONUP,
  WM_MBUTTONDOWN,WM_MBUTTONDBLCLK,WM_MBUTTONUP,
  WM_XBUTTONDOWN,WM_XBUTTONDBLCLK,WM_XBUTTONUP
 };
 for(unsigned msg:inputs)assert(runtimehot::filteredInput(msg));
 MSG wake{WM_NULL,WakeTag,0};
 assert(runtimehot::needsGetMessage(0,PM_REMOVE,&wake,0,0,WakeTag,Tactical));
 wake.wParam=WakeTag+1;assert(!runtimehot::needsGetMessage(0,PM_REMOVE,&wake,0,0,WakeTag,Tactical));
 assert(!runtimehot::needsGetMessage(0,PM_REMOVE,nullptr,0,0,WakeTag,Tactical));
 CWPRETSTRUCT mouse{0,0,0,WM_SETCURSOR};
 assert(runtimehot::needsCursorPost(0,&mouse,1));
 assert(!runtimehot::needsCursorPost(-1,&mouse,1));
 assert(!runtimehot::needsCursorPost(0,nullptr,1));
 std::printf("PASS native runtime message hotpath: checked=%u skipped_unrelated=%u retained_input=%zu WM_NULL work+cursor-post unaffected\n",checked,skipped,sizeof(inputs)/sizeof(inputs[0]));
}
