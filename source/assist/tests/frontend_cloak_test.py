"""Run the production cloak/post methods with a reentrant DWM boundary.

This checks the mailbox lock contract, not real DWM behavior or desktop input.
The native-window fixture exercises the same callback boundary on Windows.
"""
from pathlib import Path
import argparse
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path,
                    default=Path(__file__).resolve().parents[2] / 'suite/source/frontend_panes.h')
args = parser.parse_args()
source = args.source.read_text('utf-8')
cloak = ' bool cloak(Slot& s,bool value){' + source.split(' bool cloak(Slot& s,bool value){', 1)[1].split('\n static LRESULT CALLBACK paneProc', 1)[0]
post = ' bool post(unsigned index,UINT message,WPARAM a=0,LPARAM b=0){' + source.split(' bool post(unsigned index,UINT message,WPARAM a=0,LPARAM b=0){', 1)[1].split('\n bool managed(', 1)[0]
fixture = r'''
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <mutex>
using HWND=uintptr_t;using BOOL=int;using HRESULT=int;using DWORD=uint32_t;
using UINT=uint32_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using LPCVOID=const void*;
constexpr BOOL TRUE=1,FALSE=0;
static bool SUCCEEDED(HRESULT result){return result>=0;}
static unsigned posted=0,calls=0,reentries=0;static BOOL lastFlag=0;static bool failDwm=false;
static bool PostMessageW(HWND w,UINT message,WPARAM a,LPARAM b){assert(w==101&&message==87&&a==1&&b==2);++posted;return true;}
struct Composition {HRESULT(*set)(HWND,DWORD,LPCVOID,DWORD)=nullptr;};
static HRESULT callback(HWND,DWORD,LPCVOID,DWORD);
class Host {
 struct Slot {std::atomic<HWND>window{0};std::mutex mutex;bool cloakKnown=false,cloaked=false,cloakSupported=false;};
 std::array<Slot,3>slots_{};Composition api{callback};
 const Composition&compositor()const{return api;}
 public:
 Host(){slots_[0].window=101;}
 CLOAK
 POST
 bool conceal(bool value){return cloak(slots_[0],value);}
 void externalCallback(){
  // This is the callback boundary: owning-thread work needs this same mutex.
  // The old production cloak method fails here instead of hanging the test.
  assert(slots_[0].mutex.try_lock());slots_[0].mutex.unlock();
  assert(post(0,87,1,2));++reentries;
 }
 void retire(){std::lock_guard<std::mutex>lock(slots_[0].mutex);slots_[0].window=0;}
};
static Host*current;
static HRESULT callback(HWND w,DWORD attribute,LPCVOID data,DWORD bytes){
 assert(w==101&&attribute==13&&bytes==sizeof(BOOL));lastFlag=*static_cast<const BOOL*>(data);++calls;
 current->externalCallback();return failDwm?-1:0;
}
int main(){
 Host host;current=&host;
 assert(host.conceal(true)&&lastFlag==TRUE&&reentries==1&&posted==1);
 assert(host.conceal(true)&&calls==1); // a complete cached state needs no new DWM work
 assert(host.conceal(false)&&lastFlag==FALSE&&reentries==2);
 failDwm=true;assert(!host.conceal(true)&&reentries==3);
 failDwm=false;assert(host.conceal(true)&&reentries==4); // failure cannot cache success
 host.retire();assert(!host.post(0,87,1,2)&&!host.conceal(false)&&calls==4);
 std::puts("PASS: production cloak/post external-call reentry, no mailbox mutex across DWM, cache/failure retry and retired-window message rejection; no Windows compositor claim");
}
'''.replace('CLOAK', cloak).replace('POST', post)
with tempfile.TemporaryDirectory(prefix='frontend-cloak-') as temp:
    cpp, exe = Path(temp) / 'test.cpp', Path(temp) / 'test'
    cpp.write_text(fixture)
    subprocess.run(['g++', '-std=c++17', '-O2', '-UNDEBUG', '-pthread', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
