#pragma once
// Included after the three control-state variables. Disabled and partial
// groups replay the captured native path instead of retaining forced control.
static __declspec(naked) void riskControlEntry(){__asm {
 pushfd
 cmp byte ptr [riskControlActive],0
 je inactive
 popfd
 mov eax,1
 ret
inactive:
 cmp byte ptr [riskControlNativeJump],0
 jne nativeJump
 popfd
 mov eax,dword ptr ds:[0A8B238h]
 jmp dword ptr [riskControlNativeTarget]
nativeJump:
 popfd
 jmp dword ptr [riskControlNativeTarget]
}}
static __declspec(naked) void riskControlOwnerEntry(){__asm {
 pushfd
 cmp byte ptr [riskControlActive],0
 je inactive
 popfd
 mov edx,dword ptr ds:[0A83D4Ch]
 xor eax,eax
 cmp ecx,edx
 sete al
 ret
inactive:
 popfd
 push 50B6F0h
 ret
}}
