"""Run real frame-core/Runtime/Host headers with only Win32 I/O replaced."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent
source = root.parent.parent
windows = (source / "tests/winmock/windows.h").read_text()
windows += r"""
#include <atomic>
#define PAGE_READWRITE 4
#define FILE_MAP_ALL_ACCESS 0xF001F
#define FILE_MAP_READ 4
#define ERROR_ALREADY_EXISTS 183
inline void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);}
inline LONG InterlockedIncrement(volatile LONG*value){return __sync_add_and_fetch(value,1);}
inline LONG InterlockedExchange(volatile LONG*value,LONG next){return __sync_lock_test_and_set(value,next);}
extern "C" {
HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD,DWORD,const wchar_t*);
HANDLE OpenFileMappingW(DWORD,BOOL,const wchar_t*);
void*MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,SIZE_T);
BOOL UnmapViewOfFile(LPCVOID);
BOOL QueryPerformanceCounter(LARGE_INTEGER*);
}
"""
with tempfile.TemporaryDirectory(prefix="ra2_world_frame_") as tmp:
    directory = Path(tmp)
    (directory / "windows.h").write_text(windows)
    no_exceptions = directory / "world_frame_no_exceptions"
    subprocess.run([
        "g++", "-std=c++17", "-O2", "-fno-exceptions",
        str(root / "world_frame_core.cpp"), "-o", str(no_exceptions),
    ], check=True)
    subprocess.run([str(no_exceptions)], check=True)
    binary = directory / "world_frame_core"
    subprocess.run([
        "g++", "-std=c++17", "-O2", "-pthread", "-DWORLD_FRAME_PLATFORM_FIXTURE",
        "-I" + str(directory), str(root / "world_frame_core.cpp"), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
