#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <limits>
#include <string>
#include <thread>
#include "../source/neural_renderer_client.h"
static HANDLE argument(const wchar_t*value){return reinterpret_cast<HANDLE>(uintptr_t(std::wcstoull(value,nullptr,10)));}
static int fakeHelper(wchar_t**argv){
 HANDLE map=argument(argv[2]),request=argument(argv[3]),done=argument(argv[4]),parent=argument(argv[5]);auto*h=static_cast<neuralframe::Header*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(neuralframe::Header)));if(!h)return 2;uint32_t capacity=h->capacityPixels;UnmapViewOfFile(h);neuralframe::Layout layout;if(!neuralframe::layout(capacity,layout))return 2;auto*base=static_cast<uint8_t*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,size_t(layout.bytes)));if(!base)return 2;h=reinterpret_cast<neuralframe::Header*>(base);HANDLE waits[]={parent,request};
 for(;;){DWORD result=WaitForMultipleObjects(2,waits,FALSE,INFINITE);if(result!=WAIT_OBJECT_0+1)break;uint64_t frame=h->frame;assert(neuralframe::valid(*h,capacity));if(frame==4){WaitForSingleObject(parent,INFINITE);break;}if(frame==6)ExitProcess(6);
  auto*in=reinterpret_cast<uint32_t*>(base+layout.color);auto*out=reinterpret_cast<uint32_t*>(base+layout.output);size_t pixels=size_t(h->width)*h->height;for(size_t i=0;i<pixels;++i)out[i]=frame==3?0x00123456:in[i];h->resultGeneration=h->generation;h->resultFrame=frame+(frame==2?1:0);h->status=uint32_t(frame==5?neuralframe::Status::NoRuntime:neuralframe::Status::Completed);std::strcpy(h->message,frame==5?"controlled missing NR runtime":"controlled identity fixture");MemoryBarrier();if(!SetEvent(done))break;
 }UnmapViewOfFile(base);return 0;
}
static int runTests(){
 neuralclient::Client client;std::vector<uint32_t>color(16,0xffabcdef),output(2,0xdeadbeef);color[1]=0x40102030;color[2]=0x00112233;std::vector<float>motion(32),depth(16,.5f),control(16,1);control[3]=0;
 auto call=[&](uint64_t frame){return client.process(color,motion,depth,control,4,4,55,frame,true,output,client.token());};
 assert(call(1));assert(output==color&&client.diagnostics().used&&!client.blocked());
 output.assign(2,0xdeadbeef);assert(!call(2));assert(output==std::vector<uint32_t>(2,0xdeadbeef)&&client.blocked()&&!client.diagnostics().used);
 assert(!call(1));client.reset();assert(!client.blocked());
 assert(call(3));assert(output[0]==0xff123456&&output[1]==color[1]&&output[2]==color[2]&&output[3]==color[3]);
 auto frames=client.diagnostics().frames;std::fill(control.begin(),control.end(),0);output.assign(2,0xdeadbeef);assert(!call(1)&&!client.blocked()&&!client.diagnostics().used&&client.diagnostics().status==neuralframe::Status::Skipped&&client.diagnostics().frames==frames&&output==std::vector<uint32_t>(2,0xdeadbeef));depth[0]=std::numeric_limits<float>::quiet_NaN();assert(!call(1)&&!client.blocked()&&client.diagnostics().status==neuralframe::Status::InvalidInput);depth[0]=.5f;std::fill(control.begin(),control.end(),1);control[3]=0;assert(call(1)&&client.diagnostics().used&&client.diagnostics().frames==frames+1);
 auto began=std::chrono::steady_clock::now();bool success=true;std::thread worker([&]{success=call(4);});Sleep(100);client.cancel();worker.join();assert(!success&&std::chrono::duration<double>(std::chrono::steady_clock::now()-began).count()<1.5);client.reset();
 // An old UI request arriving after cancellation must not restart a child.
 const auto oldToken=client.token();client.cancel();const auto currentToken=client.token();client.reset();assert(client.token()==currentToken&&client.cancelled(oldToken));
 output.assign(2,0xdeadbeef);assert(!client.process(color,motion,depth,control,4,4,55,1,true,output,oldToken)&&!client.blocked()&&output==std::vector<uint32_t>(2,0xdeadbeef));
 assert(!call(5));assert(client.blocked()&&client.diagnostics().status==neuralframe::Status::NoRuntime);client.reset();
 assert(!call(6)&&client.blocked());client.reset();
 motion.clear();assert(!call(7)&&!client.blocked()&&client.diagnostics().status==neuralframe::Status::InvalidInput);
 std::puts("PASS: actual isolated helper IPC, exact frame/generation, stale-result rejection, sticky failure/reset, alpha/control preservation, cancellation and child-exit fallback (controlled fixture; no neural model executed)");return 0;
}
int wmain(int argc,wchar_t**argv){
 if(argc==6&&std::wstring(argv[1])==L"--ipc")return fakeHelper(argv);if(argc==2&&std::wstring(argv[1])==L"--test")return runTests();
 wchar_t temp[MAX_PATH]{},self[32768]{};assert(GetTempPathW(MAX_PATH,temp));assert(GetModuleFileNameW(nullptr,self,32768));std::wstring dir=std::wstring(temp)+L"ra2-neural-fixture-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount());assert(CreateDirectoryW(dir.c_str(),nullptr));std::wstring test=dir+L"\\Fixture.exe",helper=dir+L"\\RA2NeuralRenderer.exe";assert(CopyFileW(self,test.c_str(),TRUE)&&CopyFileW(self,helper.c_str(),TRUE));std::wstring command=L"\""+test+L"\" --test";STARTUPINFOW startup{sizeof startup};PROCESS_INFORMATION process{};assert(CreateProcessW(test.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,dir.c_str(),&startup,&process));assert(WaitForSingleObject(process.hProcess,15000)==WAIT_OBJECT_0);DWORD code=1;assert(GetExitCodeProcess(process.hProcess,&code));CloseHandle(process.hThread);CloseHandle(process.hProcess);DeleteFileW(test.c_str());DeleteFileW(helper.c_str());RemoveDirectoryW(dir.c_str());assert(code==0);std::puts("PASS: neural transport controller isolated production helper path");return 0;
}
