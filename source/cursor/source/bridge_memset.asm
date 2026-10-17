; CRT-free x86 memset for the injected Hook bridge.
; Separate MASM module avoids MSVC error C2169 (intrinsic redefinition).
.386
.model flat, C
.code
PUBLIC memset
memset PROC
    push edi
    mov edi, DWORD PTR [esp+8]  ; dst
    mov eax, DWORD PTR [esp+12] ; value
    mov ecx, DWORD PTR [esp+16] ; count
    mov edx, edi
    rep stosb
    mov eax, edx
    pop edi
    ret
memset ENDP
END
