#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <cassert>
#include <cstdio>
#include <cstring>
static bool riskControlActive,riskControlNativeJump;
static uint32_t riskControlNativeTarget;
#include "../source/control_predicate_thunks.h"
static __declspec(naked) int __fastcall invokePredicate(unsigned entry,unsigned house){__asm {
 mov eax,ecx
 mov ecx,edx
 jmp eax
}}
int main(){
 static_assert(sizeof(void*)==4,"Run the production x86 thunks");
 auto*data=VirtualAlloc((void*)0xA80000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 auto*code=VirtualAlloc((void*)0x500000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
 assert(data==(void*)0xA80000&&code==(void*)0x500000);
 *(int*)0xA8B238=3;*(uint32_t*)0xA83D4C=0x20000000;
 const unsigned char native[]={0xB8,0x55,0,0,0,0xC3};memcpy((void*)0x50B6F0,native,sizeof native);
 const unsigned char continuation[]={0x83,0xC0,100,0xC3};memcpy((void*)0x50B700,continuation,sizeof continuation);
 FlushInstructionCache(GetCurrentProcess(),code,0x10000);
 auto common=[](void*house){return invokePredicate(unsigned(uintptr_t(&riskControlEntry)),unsigned(uintptr_t(house)));};
 auto owner=[](void*house){return invokePredicate(unsigned(uintptr_t(&riskControlOwnerEntry)),unsigned(uintptr_t(house)));};
 riskControlNativeTarget=0x50B700;riskControlActive=false;riskControlNativeJump=false;
 assert(common(nullptr)==103);assert(owner((void*)0x20000000)==0x55);
 riskControlActive=true;assert(common(nullptr)==1);assert(owner((void*)0x20000000)==1);assert(owner((void*)0x20001000)==0);
 // An unsuccessful/partial restore can leave a detour reachable. Switching
 // the group off must immediately recover the captured native behavior.
 riskControlActive=false;assert(common(nullptr)==103);assert(owner((void*)0x20001000)==0x55);
 riskControlNativeJump=true;riskControlNativeTarget=0x50B6F0;assert(common(nullptr)==0x55);
 VirtualFree(code,0,MEM_RELEASE);VirtualFree(data,0,MEM_RELEASE);
 puts("PASS: actual x86 control thunks, enabled/disabled/partial-restore and captured-jump fallback; synthetic native bodies only");
}
