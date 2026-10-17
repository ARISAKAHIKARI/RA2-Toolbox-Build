#pragma once
#include <windows.h>
// v1.3.0: per-thread native cursor, post-WM_SETCURSOR repair, menu suspend.
// The new mapping name and magic prevent reusing an older Hook DLL.
constexpr DWORD MAGIC=0x4355523A;
constexpr int NATIVE_MAX_DIM=256;
constexpr unsigned RESOURCE_MAX_BYTES=4*1024*1024;
struct Shared {
 DWORD magic,pid,owner;
 volatile LONG command,state,error;
 volatile DWORD heartbeat;
 volatile LONG observedRef,hiddenRef,rehideCount,lastRehideReason;
 // Produced ONLY by the game-thread bridge (no cross-process VM_READ needed).
 volatile LONG sampleSeq,sampledGameW,sampledGameH;
 volatile LONG sampledFrame,sampledFirst,sampledCount,sampledTick,sampledHotX,sampledHotY;
 volatile LONG sampleAttempts,sampleSuccess,sampleReadError;
 volatile LONG sampleIntervalMs,nativeCacheHits,nativeCacheMisses;
 volatile LONG cursorCanvasW,cursorCanvasH;
 volatile LONG cursorFrameCount,sampledCursorID;
 // One-shot transfer of the game's original SHA and palette, then cached.
 volatile LONG resourceRequest,resourceState,resourceSeq;
 volatile DWORD resourceBytes;
 DWORD resourcePalette[256];
 BYTE resourcePixels[RESOURCE_MAX_BYTES];
 // nativeMode=0 normal cursor, 1 game animation, 2 OS-dialog menu.
 volatile LONG menuSuspend,postSetCursorRepairs;
 DWORD gameWindow;
 // Host produces complete color cursor frames. seq odd=writing, even=stable.
 volatile LONG nativeBlank,nativeEnabled,nativeSeq,nativeAck,nativeVisible;
 volatile LONG nativeW,nativeH,nativeHotX,nativeHotY;
 DWORD nativePixels[NATIVE_MAX_DIM*NATIVE_MAX_DIM];
};
inline void mapName(wchar_t* b,DWORD pid){wsprintfW(b,L"Local\\RA2CursorBridge_v154_%lu",pid);}
