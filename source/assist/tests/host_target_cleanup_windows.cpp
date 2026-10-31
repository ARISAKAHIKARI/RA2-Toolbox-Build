#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../shared/source/host_game_input.h"
#include <cassert>
#include <cstdio>
static LRESULT CALLBACK message(int c,WPARAM w,LPARAM l){return CallNextHookEx(nullptr,c,w,l);}
int main(){
 // Windows has already removed an HHOOK when its thread/process exits.
 // Reproduce the invalid-handle result explicitly, then reuse more than
 // eight targets without accumulating occupied slots.
 for(unsigned i=0;i<12;++i){auto&t=gameinput::host().targets[0];assert(t.pid==0);
  t.pid=GetCurrentProcessId();t.thread=GetCurrentThreadId();
  t.message=SetWindowsHookExW(WH_GETMESSAGE,message,nullptr,t.thread);assert(t.message);
  assert(UnhookWindowsHookEx(t.message));assert(gameinput::clear(t));assert(t.pid==0&&!t.message);
 }
 puts("PASS: twelve stale HHOOK cleanup/reuse cycles; no game process or injection");
}
