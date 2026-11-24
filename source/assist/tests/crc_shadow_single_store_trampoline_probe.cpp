// Research-only x86 MSVC/clang-cl code-generation probe.
// This is not included by production Runtime.
extern "C" volatile unsigned char crcMapSkipStore;
extern "C" unsigned __cdecl GuardedMapCrcRevealStore(unsigned house, unsigned savedEBX);

#define CRC_MAP_REVEAL_CALLBACK \
    __asm { pushfd } __asm { pushad } __asm { mov ebx,esp } \
    __asm { sub esp,528 } __asm { and esp,-16 } __asm { fxsave [esp] } \
    __asm { cld } __asm { fninit } __asm { fldcw [esp] } \
    __asm { mov dword ptr [esp+512],1F80h } __asm { ldmxcsr [esp+512] } \
    __asm { push dword ptr [ebx+16] } __asm { push dword ptr [ebx+24] } \
    __asm { call GuardedMapCrcRevealStore } __asm { add esp,8 } \
    __asm { mov dword ptr [ebx+16],eax } __asm { fxrstor [esp] } \
    __asm { mov esp,ebx } __asm { popad } __asm { popfd }

extern "C" __declspec(naked) void crcMapRevealSingleEntry() {
    CRC_MAP_REVEAL_CALLBACK
    __asm {
        pushfd
        cmp byte ptr [crcMapSkipStore], 0
        jne skip_original_store
        mov byte ptr [ecx+241h], bl
    skip_original_store:
        mov byte ptr [crcMapSkipStore], 0
        popfd
        push 577DB6h
        ret
    }
}
#undef CRC_MAP_REVEAL_CALLBACK
