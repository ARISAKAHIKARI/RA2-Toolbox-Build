"""Run the production process reader against bounded Win32 read doubles."""
from pathlib import Path
import json
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
windows = r'''
#pragma once
#include <cstddef>
#include <cstdint>
using DWORD=uint32_t;using HANDLE=void*;using SIZE_T=size_t;using BOOL=int;
constexpr BOOL FALSE=0,TRUE=1;
constexpr DWORD PROCESS_QUERY_LIMITED_INFORMATION=0x1000,PROCESS_VM_READ=0x10,PROCESS_QUERY_INFORMATION=0x400;
constexpr DWORD STILL_ACTIVE=259,ERROR_INVALID_HANDLE=6,ERROR_ACCESS_DENIED=5,ERROR_INVALID_PARAMETER=87,ERROR_PARTIAL_COPY=299,ERROR_SUCCESS=0;
struct MEMORY_BASIC_INFORMATION {DWORD State=0,Protect=0;};
extern DWORD fakeLastError;
inline DWORD GetLastError(){return fakeLastError;}
inline HANDLE OpenProcess(DWORD,BOOL,DWORD pid){return pid==7?reinterpret_cast<HANDLE>(7):nullptr;}
inline BOOL CloseHandle(HANDLE){return TRUE;}
inline BOOL GetExitCodeProcess(HANDLE,DWORD*code){*code=STILL_ACTIVE;return TRUE;}
inline BOOL QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*,DWORD*){return TRUE;}
inline SIZE_T VirtualQueryEx(HANDLE,const void*,MEMORY_BASIC_INFORMATION*info,SIZE_T size){info->State=0x1000;info->Protect=0x04;return size;}
BOOL ReadProcessMemory(HANDLE,const void*,void*,SIZE_T,SIZE_T*);
'''
code = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <new>
static size_t allocationCalls=0;
void* operator new(size_t n){++allocationCalls;if(void*p=std::malloc(n))return p;throw std::bad_alloc();}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
#include @READER@
DWORD fakeLastError=0;
static bool completePage=false;
static bool batchFail=false;
static bool wideRange=false;static uint8_t memoryVersion=0;
static int pageAttempts=0,fieldAttempts=0;
BOOL ReadProcessMemory(HANDLE,const void*at,void*out,SIZE_T size,SIZE_T*got){
 const uintptr_t address=reinterpret_cast<uintptr_t>(at);
 if(batchFail&&size>4096){*got=0;fakeLastError=ERROR_PARTIAL_COPY;return FALSE;}
 if(address==0x12000&&size==4096){++pageAttempts;if(!completePage){*got=2048;fakeLastError=ERROR_PARTIAL_COPY;return FALSE;}}
 else if(size<4096)++fieldAttempts;
 if(address<0x12000||(!wideRange&&address+size>0x14000)||(!completePage&&address+size>0x12800&&address<0x13000)){
  *got=0;fakeLastError=ERROR_PARTIAL_COPY;return FALSE;
 }
 auto*bytes=static_cast<uint8_t*>(out);
 for(size_t i=0;i<size;++i)bytes[i]=uint8_t(address+i+memoryVersion);
 *got=size;return TRUE;
}
int main(){
 external::ProcessReader reader;assert(reader.open(7));
 uint8_t value[4]{};
 for(int n=0;n<10;++n){
  assert(reader.read(0x12010+n*4,value,sizeof value));
  assert(value[0]==uint8_t(0x10+n*4)&&value[3]==uint8_t(0x13+n*4));
 }
 assert(pageAttempts==1&&fieldAttempts==10);
 assert(reader.pageMisses==10&&reader.pageFallbacks==10&&reader.pageHits==0&&reader.reads==11&&reader.errors==0);
 // A full-page failure cannot poison later direct field reads in this sample.
 completePage=true;assert(reader.read(0x12080,value,sizeof value));
 assert(pageAttempts==1&&fieldAttempts==11);
 reader.begin();assert(reader.read(0x12080,value,sizeof value));
 assert(pageAttempts==2&&reader.pageMisses==1&&reader.pageHits==0);
 assert(reader.read(0x120a0,value,sizeof value));
 assert(pageAttempts==2&&reader.pageHits==1&&reader.reads==1);
 // A new sampling epoch or re-open retries the full page, without stale data.
 assert(reader.open(7));assert(reader.read(0x120c0,value,sizeof value));
 assert(pageAttempts==3&&reader.pageHits==0);
 completePage=false;reader.begin();assert(!reader.read(0x12ffe,value,sizeof value));
 assert(reader.errors==1&&reader.lastFailedAddress==0x12ffe&&reader.lastRegionQueried);
 assert(reader.read(0x12008,value,sizeof value));assert(value[0]==8);
 // Stable sampling addresses retain their storage, never values from an old
 // observation. Every first page access after begin still reads game memory.
 completePage=true;wideRange=true;
 for(int pass=0;pass<100;++pass){memoryVersion=uint8_t(pass);reader.begin();
  for(unsigned page=0;page<8;++page){assert(reader.read(0x20000+page*4096+16,value,4));assert(value[0]==uint8_t(16+memoryVersion));}
  assert(reader.reads==8&&reader.pageMisses==8&&reader.pageHits==0);
  assert(reader.read(0x20010,value,4));assert(reader.pageHits==1);
 }
 const auto allocations=reader.pageBufferAllocations();
 const auto allAllocations=allocationCalls;
 for(int pass=0;pass<100;++pass){reader.begin();for(unsigned page=0;page<8;++page)assert(reader.read(0x20000+page*4096+16,value,4));}
 assert(reader.pageBufferAllocations()==allocations&&allocationCalls==allAllocations);
 // Address churn rekeys bounded existing storage instead of exceeding 32 MiB.
 reader.begin();for(unsigned page=0;page<8200;++page)assert(reader.read(0x100000+page*4096+16,value,4));
 assert(reader.cachedPageStorage()==8192&&reader.pageBufferAllocations()==8192);
 memoryVersion=201;reader.begin();assert(reader.read(0x100010,value,4)&&value[0]==uint8_t(16+201));
 assert(reader.pageBufferAllocations()==8192);
 // Sixteen adjacent actor pages now need one RPM, with current-epoch bytes.
 std::vector<uint32_t> actors;for(unsigned n=0;n<16;++n)actors.push_back(0x400000+n*4096+16);
 memoryVersion=11;reader.begin();reader.prefetch(actors,0x800);assert(reader.reads==1&&reader.prefetchedPages==16);
 for(auto at:actors){assert(reader.read(at,value,4)&&value[0]==27);assert(reader.read(at+0x600,value,4));}
 assert(reader.reads==1&&reader.pageHits==32);
 memoryVersion=12;reader.begin();reader.prefetch(actors,0x800);for(auto at:actors)assert(reader.read(at,value,4)&&value[0]==28);
 // A failed speculative batch leaves every good page eligible for lazy read.
 batchFail=true;reader.begin();reader.prefetch(actors,0x800);assert(reader.prefetchMisses==1&&reader.errors==0);
 for(auto at:actors)assert(reader.read(at,value,4)&&value[0]==28);
 assert(reader.reads==17&&reader.pageMisses==16);batchFail=false;
 reader.close();assert(reader.cachedPageStorage()==0&&!reader.read(0x100010,value,4));
 std::cout<<"PASS ProcessReader: partial-page fallback, fresh epoch values, zero warm page-buffer allocations, bounded address churn, coalesced actor reads/failure fallback and process-close invalidation\n";
}
'''.replace('@READER@', json.dumps(str(root / 'external_reader/source/process_reader.h')))
with tempfile.TemporaryDirectory(prefix='ra2-reader-pages-') as tmp:
    directory=Path(tmp)
    (directory/'windows.h').write_text(windows)
    (directory/'test.cpp').write_text(code)
    subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-I'+str(directory),
                    str(directory/'test.cpp'),'-o',str(directory/'test')],check=True)
    subprocess.run([str(directory/'test')],check=True,timeout=20)
