"""Execute the production Host placement diagnostics with bounded control/memory mocks."""
from pathlib import Path
import argparse,hashlib,json,re,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[2];source=root/'assist/source/activation_host.h';body=source.read_text()
shared=sorted(set(re.findall(r'shared->(\w+)',body))-{'debugSnapshot','debugSnapshotSeq','flags'})
incoming=sorted(set(re.findall(r'incoming\.(\w+)',body)))
methods=sorted(set(re.findall(r'extra\.(\w+)\(',body)))
extra=sorted(set(re.findall(r'extra\.(\w+)',body))-set(methods))
scalars=['settingsRevision','techMode','techImplementation','buildTimeTooltipEnabled','pid','lastData','bridgeBlocked','buildAnywhereMode']
prefix=r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <string>
#include <vector>
#include "debug_entry_policy.h"
#include "activation_diagnostic.h"
using namespace assist;using DWORD=uint32_t;using LONG=int32_t;
static LONG InterlockedCompareExchange(volatile LONG*p,LONG,LONG){return *p;}static void MemoryBarrier(){}
static std::vector<std::wstring>messages;static void critical(const std::wstring&s){messages.push_back(s);}
static std::wstring hex32(uint32_t n){wchar_t b[16];swprintf(b,16,L"0x%08X",n);return b;}
static uint32_t flags(){return 65536;}
namespace nativesell {static const wchar_t*failureText(int){return nullptr;}}
static struct {int reads=0;uint32_t fail=0;bool readLive(uint32_t at,void*out,size_t n){++reads;if(at==fail)return false;memset(out,0xCC,n);return true;}}externalReader;
'''
prefix+='static int '+','.join(scalars)+';\n'
prefix+='static struct{'+''.join('int '+n+'=0;' for n in shared)+'uint32_t flags=65536;volatile LONG debugSnapshotSeq=2;debugentries::Snapshot debugSnapshot;}data,*shared=&data;\n'
prefix+='static struct{'+''.join('int '+n+'=0;' for n in incoming)+'}incoming;\n'
prefix+='static struct{'+''.join('int '+n+'=0;' for n in extra)+''.join('int '+n+'(){return 0;}' for n in methods)+'}extra;\n'
code=prefix+body+r'''
int main(){
 pid=7;buildAnywhereMode=1;extra.placementImplementation=2;incoming.status=1;incoming.commandStatus=1;incoming.placementStatus=-2;incoming.session=3;
 data.debugSnapshot.count=debugentries::Count;
 for(unsigned i=0;i<debugentries::Count;++i){auto&o=data.debugSnapshot.entries[i];o.id=debugentries::Entries[i].id;o.signature=debugentries::Signature::Mismatch;o.hook=debugentries::Hook::Unavailable;o.reason=debugentries::Reason::Signature;o.error=-2;o.tick=1000;}
 logFeatureActivation(1000);assert(messages.size()==2&&externalReader.reads==5);
 assert(messages[0].find(L"placementMode=2")!=std::wstring::npos&&messages[1].find(L"placement-admission mode=2")!=std::wstring::npos);
 assert(messages[1].find(L"entry=0x004A8EB0 signature=3 hook=5 reason=1 error=-2")!=std::wstring::npos);
 assert(messages[1].find(L"probe-0x004A918E=CCCC")!=std::wstring::npos);
 logFeatureActivation(1008);logFeatureActivation(1100);logFeatureActivation(6000);assert(messages.size()==2&&externalReader.reads==5);
 extra.placementImplementation=0;logFeatureActivation(6250);assert(messages.size()==4&&externalReader.reads==10&&messages.back().find(L"mode=0")!=std::wstring::npos);
 auto&o=data.debugSnapshot.entries[debugentries::index(debugentries::id(debugentries::Kind::Patch,0x4A8EB0))];o.reason=debugentries::Reason::Unreadable;
 logFeatureActivation(6600);assert(messages.size()==6&&messages.back().find(L"reason=2")!=std::wstring::npos);
 data.debugSnapshotSeq=3;logFeatureActivation(6950);assert(messages.size()==8&&messages.back().find(L"debugValid=0")!=std::wstring::npos&&messages.back().find(L"entry=")==std::wstring::npos);
 incoming.placementStatus=2;logFeatureActivation(7300);assert(messages.size()==9&&externalReader.reads==20);
 data.debugSnapshotSeq=4;externalReader.fail=0x4A918E;incoming.placementStatus=-2;logFeatureActivation(7650);
 assert(messages.size()==11&&externalReader.reads==25&&messages.back().find(L"probe-0x004A918E=unreadable")!=std::wstring::npos);
 std::cout<<"PASS production placement diagnostics: mode/status/entry reason, bounded Host-only bytes, stable-state no-op, mode/reason changes, torn snapshot rejection and unreadable probe\n";
}
'''
cpp=a.out/'diagnostic.cpp';cpp.write_text(code);exe=a.out/'diagnostic';r=subprocess.run(['g++','-std=c++17','-O1','-I'+str(root/'shared/source'),'-I'+str(root/'assist/source'),str(cpp),'-o',str(exe)],capture_output=True,text=True);(a.out/'compile.log').write_text(r.stdout+r.stderr)
if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(exe)],capture_output=True,text=True);(a.out/'result.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr,end='')
(a.out/'result.json').write_text(json.dumps({'pass':r.returncode==0,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'game_tested':False,'windows_executed':False})+'\n');raise SystemExit(r.returncode)
