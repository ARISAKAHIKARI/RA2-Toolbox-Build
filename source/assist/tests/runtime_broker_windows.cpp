#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cwchar>
#include <cstdio>
#include <string>
#include "runtime_broker_fixture.h"
#include "../../shared/source/runtime_broker_client.h"
#include "../../shared/source/runtime_protocol.h"
#include "../../shared/source/game_entry_api.h"
#include "../source/protocol.h"
static_assert(sizeof(void*)==8,"Cross-bitness controller must be x64");
static void require(bool value,const char*text){if(!value){std::fprintf(stderr,"FAIL: %s (Win32=%lu)\n",text,(unsigned long)GetLastError());ExitProcess(1);}}
template<class Predicate>static bool waitFor(Predicate predicate,DWORD milliseconds=5000){DWORD start=GetTickCount();do{if(predicate())return true;Sleep(10);}while(DWORD(GetTickCount()-start)<milliseconds);return false;}
// Fixture driver only: production Host calls return immediately and advance
// from its existing timer. This standalone controller polls outside Host locks.
template<class Operation>static bool complete(Operation operation,DWORD milliseconds=8000){
 const DWORD start=GetTickCount();for(;;){
  const bool ready=operation();const DWORD error=GetLastError();
  if(ready||(error!=ERROR_IO_PENDING&&error!=ERROR_BUSY)){SetLastError(error);return ready;}
  if(DWORD(GetTickCount()-start)>=milliseconds){SetLastError(ERROR_TIMEOUT);return false;}Sleep(4);
 }
}
static std::wstring sibling(const wchar_t*name){wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);require(n&&n<32768,"controller image path");std::wstring out(path,n);out.resize(out.find_last_of(L"\\/")+1);return out+name;}
static PROCESS_INFORMATION start(const std::wstring&image,const std::wstring&arguments){
 std::wstring command=L"\""+image+L"\" "+arguments;STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION process{};
 require(CreateProcessW(image.c_str(),&command[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=FALSE,"start fixture process");CloseHandle(process.hThread);return process;
}
struct Fixture {
 HANDLE mapping=nullptr,controlMapping=nullptr;brokerfixture::Probe*probe=nullptr;runtime::Control*control=nullptr;PROCESS_INFORMATION target{};
 void open(){
  wchar_t name[96];brokerfixture::name(name,GetCurrentProcessId());mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(brokerfixture::Probe),name);
  require(mapping&&GetLastError()!=ERROR_ALREADY_EXISTS,"private fixture mapping");probe=(brokerfixture::Probe*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*probe));require(probe,"fixture view");*probe={};probe->owner=GetCurrentProcessId();
  target=start(sibling(L"BrokerTarget32.exe"),std::to_wstring(GetCurrentProcessId()));require(waitFor([&]{return probe->window&&probe->pid==target.dwProcessId;}),"target window ready");
 }
 void controlOpen(){
  wchar_t name[96];runtime::mapName(name,probe->pid,GetCurrentProcessId());controlMapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(runtime::Control),name);
  require(controlMapping,"runtime mapping");control=(runtime::Control*)MapViewOfFile(controlMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*control));require(control,"runtime view");*control={};control->magic=runtime::Magic;control->version=runtime::Version;control->pid=probe->pid;control->owner=GetCurrentProcessId();
 }
 void send(unsigned sequence){
  HWND window=(HWND)(uintptr_t)probe->window;
  require(PostMessageW(window,brokerfixture::Message,sequence,GetCurrentProcessId())!=FALSE,"post fixture message");require(waitFor([&]{return probe->ack==LONG(sequence);}),"fixture target ack");
  DWORD_PTR result=0;require(SendMessageTimeoutW(window,brokerfixture::SentMessage,sequence,GetCurrentProcessId(),SMTO_ABORTIFHUNG|SMTO_BLOCK,2000,&result)!=0,"send fixture message");
 }
 void close(){
  if(target.hProcess){PostMessageW((HWND)(uintptr_t)probe->window,WM_CLOSE,0,0);require(WaitForSingleObject(target.hProcess,3000)==WAIT_OBJECT_0,"target closes");CloseHandle(target.hProcess);target={};}
  if(control)UnmapViewOfFile(control);if(controlMapping)CloseHandle(controlMapping);if(probe)UnmapViewOfFile(probe);if(mapping)CloseHandle(mapping);
 }
};
static int ownerChild(DWORD parent){
 Fixture f;f.open();f.controlOpen();auto&client=runtimebroker::client();uint32_t token=0,generation=0;
 require(complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,token,generation);}),"child owner installs actual hook pair");f.send(1);
 require(waitFor([&]{return f.probe->messageCalls&&f.probe->postCalls;}),"child owner callback observations");
 wchar_t name[96];brokerfixture::name(name,parent);HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);auto*report=mapping?(brokerfixture::Probe*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(brokerfixture::Probe)):nullptr;
 require(report,"child owner report");report->childPID=GetCurrentProcessId();report->childTargetPID=f.probe->pid;report->childThread=f.probe->thread;report->childWindow=f.probe->window;report->childBrokerPID=client.processId();InterlockedExchange(&report->childReady,1);
 for(;;)Sleep(1000); // The parent intentionally terminates this controlled owner.
}
int wmain(int argc,wchar_t**argv){
 if(argc==3&&wcscmp(argv[1],L"--owner-child")==0)return ownerChild(DWORD(std::wcstoul(argv[2],nullptr,10)));
 Fixture f;f.open();auto&client=runtimebroker::client();uint32_t token=0,generation=0;
 require(complete([&]{return client.transport(runtime::Version,sizeof(runtime::Control),gameentry::Version,sizeof(gameentry::Request));}),"x64 host launches x86 metadata broker through inherited handles");
 uint32_t version=0,bytes=0;require(complete([&]{return client.identity(version,bytes,assist::MappingPrefix.text);})&&version==assist::Version&&bytes==sizeof(assist::Shared),"strict assist metadata");
 require(!complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,token,generation);}),"missing runtime control mapping rejected");
 f.controlOpen();f.control->owner+=1;require(!complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,token,generation);}),"wrong control owner rejected");f.control->owner-=1;
 require(!complete([&]{return client.install(f.probe->pid,f.probe->thread+1,f.probe->window,token,generation);}),"wrong thread rejected");
 require(!complete([&]{return client.install(f.probe->pid,f.probe->thread,0,token,generation);}),"missing window rejected");
 require(complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,token,generation);})&&token&&client.alive(generation),"actual x86 hook pair installed");
 uint32_t repeated=0,repeatedGeneration=0;require(complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,repeated,repeatedGeneration);})&&repeated==token&&repeatedGeneration==generation,"same target install idempotent");
 f.send(1);require(waitFor([&]{return f.probe->messageCalls==1&&f.probe->postCalls==1;}),"both real callbacks run inside x86 target");
 require(complete([&]{return client.remove(f.probe->pid,token,generation);}),"actual hook pair removed");f.send(2);Sleep(100);require(f.probe->messageCalls==1&&f.probe->postCalls==1,"removed callbacks do not run");
 require(complete([&]{return client.shutdown();}),"helper clean shutdown");uint32_t oldGeneration=generation;
 require(complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,token,generation);})&&generation!=oldGeneration,"helper restart has new identity");f.send(3);
 require(waitFor([&]{return f.probe->messageCalls==2&&f.probe->postCalls==2;}),"restarted helper callbacks");
 HANDLE broker=OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,client.processId());require(broker,"owned helper termination handle");require(TerminateProcess(broker,23)&&WaitForSingleObject(broker,3000)==WAIT_OBJECT_0,"helper crash fixture");CloseHandle(broker);
 require(!client.alive(generation),"dead helper detected");f.send(4);Sleep(100);require(f.probe->messageCalls==2&&f.probe->postCalls==2,"helper exit removes target hooks");
 require(complete([&]{return client.install(f.probe->pid,f.probe->thread,f.probe->window,token,generation);}),"helper restarts after crash");f.send(5);require(waitFor([&]{return f.probe->messageCalls==3&&f.probe->postCalls==3;}),"crash recovery callbacks");require(complete([&]{return client.shutdown();}),"release runtime before mismatch test");
 auto runtime=sibling(L"GameRuntime.dll"),saved=sibling(L"GameRuntimeGood.dll"),bad=sibling(L"GameRuntimeBad.dll");
 require(MoveFileW(runtime.c_str(),saved.c_str())!=FALSE,"save good fixture runtime");require(CopyFileW(bad.c_str(),runtime.c_str(),FALSE)!=FALSE,"place mismatched fixture runtime");
 require(!complete([&]{return client.ensure();})&&GetLastError()==ERROR_REVISION_MISMATCH,"mismatched runtime fails before installing hooks");
 require(DeleteFileW(runtime.c_str())&&MoveFileW(saved.c_str(),runtime.c_str()),"restore fixture runtime");require(complete([&]{return client.ensure();})&&complete([&]{return client.shutdown();}),"recover after runtime mismatch");
 PROCESS_INFORMATION child=start(sibling(L"runtime-broker-test.exe"),L"--owner-child "+std::to_wstring(GetCurrentProcessId()));require(waitFor([&]{return f.probe->childReady!=0;},10000),"independent owner ready");
 wchar_t childName[96];brokerfixture::name(childName,f.probe->childPID);HANDLE childMap=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,childName);auto*childProbe=childMap?(brokerfixture::Probe*)MapViewOfFile(childMap,FILE_MAP_ALL_ACCESS,0,0,sizeof(brokerfixture::Probe)):nullptr;require(childProbe,"child target observations");
 HANDLE childBroker=OpenProcess(SYNCHRONIZE,FALSE,f.probe->childBrokerPID),childTarget=OpenProcess(SYNCHRONIZE,FALSE,f.probe->childTargetPID);require(childBroker&&childTarget,"independent owner children");LONG messages=childProbe->messageCalls,posts=childProbe->postCalls;
 require(TerminateProcess(child.hProcess,24)&&WaitForSingleObject(child.hProcess,3000)==WAIT_OBJECT_0,"controlled host death");require(WaitForSingleObject(childBroker,4000)==WAIT_OBJECT_0,"broker exits on inherited owner process death");
 require(PostMessageW((HWND)(uintptr_t)f.probe->childWindow,brokerfixture::Message,2,f.probe->childPID)!=FALSE,"surviving child target receives message");require(waitFor([&]{return childProbe->ack==2;}),"surviving child target ack");
 DWORD_PTR result=0;require(SendMessageTimeoutW((HWND)(uintptr_t)f.probe->childWindow,brokerfixture::SentMessage,2,f.probe->childPID,SMTO_ABORTIFHUNG|SMTO_BLOCK,2000,&result)!=0,"surviving child target sent message");Sleep(100);
 require(childProbe->messageCalls==messages&&childProbe->postCalls==posts,"owner death leaves no active hooks");PostMessageW((HWND)(uintptr_t)f.probe->childWindow,WM_CLOSE,0,0);require(WaitForSingleObject(childTarget,3000)==WAIT_OBJECT_0,"child target cleanup");
 UnmapViewOfFile(childProbe);CloseHandle(childMap);CloseHandle(childBroker);CloseHandle(childTarget);CloseHandle(child.hProcess);f.close();
 std::puts("PASS: actual x64-to-x86 broker metadata/private IPC, invalid identity rejection, real hook pair callbacks/unhook, clean/crash restart, runtime mismatch and owner-death teardown");return 0;
}
