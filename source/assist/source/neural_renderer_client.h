#pragma once
// SPDX-License-Identifier: MIT
// Optional, isolated x64 renderer. This client never loads the NVIDIA SDK.
#include "../../shared/source/neural_frame_protocol.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <new>
#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif
namespace neuralclient {
struct Diagnostics {bool used=false;uint64_t frames=0;double milliseconds=0;neuralframe::Status status=neuralframe::Status::Idle;std::string reason="DLSS 5 is disabled";};
class Client {
 Diagnostics diagnostics_;bool quarantined_=false;std::atomic<uint64_t>cancelSerial_{0};std::mutex mutex_;
#ifdef _WIN64
 HANDLE mapping_=nullptr,request_=nullptr,complete_=nullptr,child_=nullptr,job_=nullptr;
 void*view_=nullptr;uint32_t capacity_=0;bool warmed_=false;neuralframe::Layout layout_{};
 void close(){
  // The job owns only our helper. Closing it also handles a stuck GPU call;
  // the GUI and its normal renderer never wait indefinitely for that child.
  if(job_)CloseHandle(job_);job_=nullptr;
  if(child_)CloseHandle(child_);child_=nullptr;
  if(view_)UnmapViewOfFile(view_);view_=nullptr;
  for(auto h:{mapping_,request_,complete_})if(h)CloseHandle(h);
  mapping_=request_=complete_=nullptr;capacity_=0;warmed_=false;layout_={};
 }
 bool fail(const char*reason,bool sticky=false,neuralframe::Status status=neuralframe::Status::BackendFailed){diagnostics_.used=false;diagnostics_.reason=reason;diagnostics_.status=status;quarantined_|=sticky;close();return false;}
 static std::wstring handleText(HANDLE h){return std::to_wstring(uint64_t(uintptr_t(h)));}
 bool start(uint32_t pixels){
  if(quarantined_)return false;if(child_&&capacity_>=pixels)return true;close();
  // Grow in coarse steps, bounded by the same 16M-pixel limit as the renderer.
  uint32_t capacity=65536;while(capacity<pixels&&capacity<neuralframe::MaximumPixels)capacity*=2;
  if(!neuralframe::layout(capacity,layout_))return fail("DLSS 5 frame dimensions exceed the helper budget");
  wchar_t path[32768]{};DWORD n=GetModuleFileNameW(nullptr,path,32768);
  if(!n||n>=32768)return fail("DLSS 5 helper path is unavailable");std::wstring base(path,n);auto slash=base.find_last_of(L"\\/");if(slash==std::wstring::npos)return fail("DLSS 5 helper path is unavailable");base.resize(slash);
  std::wstring exe=base+L"\\RA2NeuralRenderer.exe";
  if(GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES)return fail("DLSS 5 helper is not installed",false,neuralframe::Status::NoRuntime);
  SECURITY_ATTRIBUTES security{sizeof security,nullptr,TRUE};
  mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,&security,PAGE_READWRITE,DWORD(layout_.bytes>>32),DWORD(layout_.bytes),nullptr);
  request_=CreateEventW(&security,FALSE,FALSE,nullptr);complete_=CreateEventW(&security,FALSE,FALSE,nullptr);
  HANDLE parent=OpenProcess(SYNCHRONIZE,TRUE,GetCurrentProcessId());
  if(!mapping_||!request_||!complete_||!parent){if(parent)CloseHandle(parent);return fail("DLSS 5 helper IPC allocation failed");}
  view_=MapViewOfFile(mapping_,FILE_MAP_ALL_ACCESS,0,0,size_t(layout_.bytes));
  if(!view_){CloseHandle(parent);return fail("DLSS 5 shared frame mapping failed");}
  new(view_)neuralframe::Header();static_cast<neuralframe::Header*>(view_)->capacityPixels=capacity;capacity_=capacity;
  SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<uint8_t>attributes(bytes);auto list=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
  if(!bytes||!InitializeProcThreadAttributeList(list,1,0,&bytes)){CloseHandle(parent);return fail("DLSS 5 helper process attributes failed");}
  HANDLE inherited[]={mapping_,request_,complete_,parent};
  bool updated=UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr)!=FALSE;
  STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof startup;startup.lpAttributeList=list;PROCESS_INFORMATION process{};
  std::wstring command=L"\""+exe+L"\" --ipc "+handleText(mapping_)+L" "+handleText(request_)+L" "+handleText(complete_)+L" "+handleText(parent);
  job_=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  bool jobReady=job_&&SetInformationJobObject(job_,JobObjectExtendedLimitInformation,&limits,sizeof limits);
  bool created=updated&&jobReady&&CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,base.c_str(),&startup.StartupInfo,&process);
  DeleteProcThreadAttributeList(list);CloseHandle(parent);
  if(!created)return fail("DLSS 5 helper could not be started");
  child_=process.hProcess;
  if(!AssignProcessToJobObject(job_,child_)){TerminateProcess(child_,1);CloseHandle(process.hThread);return fail("DLSS 5 helper lifetime ownership failed");}
  DWORD resumed=ResumeThread(process.hThread);CloseHandle(process.hThread);if(resumed==DWORD(-1))return fail("DLSS 5 helper could not be resumed");
  return true;
 }
#endif
public:
 Client()=default;Client(const Client&)=delete;Client&operator=(const Client&)=delete;
 ~Client(){cancel();reset();}
 const Diagnostics&diagnostics()const{return diagnostics_;}
 bool blocked()const{return quarantined_;} // worker-thread query only
 bool canAttempt()const{return !quarantined_;}
 uint64_t cancellationToken()const{return cancelSerial_.load(std::memory_order_relaxed);}
 uint64_t token()const{return cancellationToken();}
 bool cancelled(uint64_t token)const{return cancellationToken()!=token;}
 // Cancellation can be requested from the UI thread. Only the render worker
 // closes handles, after its bounded wait notices the changed serial.
 void cancel(){cancelSerial_.fetch_add(1,std::memory_order_relaxed);
#ifdef _WIN64
  std::unique_lock<std::mutex>lock(mutex_,std::try_to_lock);if(lock.owns_lock())close();
#endif
 }
 // Explicitly toggling the feature off releases the child and permits a retry.
 // reset/destruction run only on the worker, or after that worker was joined.
 void reset(){
  std::lock_guard<std::mutex>lock(mutex_);
#ifdef _WIN64
  close();
#endif
  quarantined_=false;diagnostics_.used=false;diagnostics_.status=neuralframe::Status::Idle;diagnostics_.reason="DLSS 5 is disabled";
 }
 bool process(const std::vector<uint32_t>&color,const std::vector<float>&motion,const std::vector<float>&depth,const std::vector<float>&control,
              uint32_t width,uint32_t height,uint64_t generation,uint64_t frame,bool resetHistory,std::vector<uint32_t>&output,uint64_t expectedToken){
  std::lock_guard<std::mutex>lock(mutex_);
  diagnostics_.used=false;
#ifdef _WIN64
  const auto cancelSerial=expectedToken;
  if(cancelled(cancelSerial))return fail("DLSS 5 request was cancelled before submission");
  struct CancelClose {Client&client;uint64_t serial;~CancelClose(){if(client.cancelSerial_.load(std::memory_order_relaxed)!=serial)client.close();}}cancelClose{*this,cancelSerial};
  uint64_t pixels=uint64_t(width)*height;
  if(!neuralframe::dimensions(width,height,neuralframe::MaximumPixels)||color.size()!=pixels||motion.size()!=pixels*2||depth.size()!=pixels||control.size()!=pixels){diagnostics_.status=neuralframe::Status::InvalidInput;diagnostics_.reason="DLSS 5 inputs are incomplete";return false;}
  if(quarantined_){diagnostics_.reason="DLSS 5 stopped after a helper failure; toggle off before retrying";return false;}
  bool eligible=false;for(size_t i=0;i<pixels;++i){if(!std::isfinite(control[i])||control[i]<0||control[i]>1||!std::isfinite(depth[i])||depth[i]<0||depth[i]>1||!std::isfinite(motion[i*2])||!std::isfinite(motion[i*2+1])||std::abs(motion[i*2])>32768||std::abs(motion[i*2+1])>32768){diagnostics_.status=neuralframe::Status::InvalidInput;diagnostics_.reason="DLSS 5 motion, depth or confidence input is invalid";return false;}eligible|=control[i]>0&&(color[i]>>24)==255;}
  if(!eligible){diagnostics_.status=neuralframe::Status::Skipped;diagnostics_.reason="DLSS 5 skipped this fully protected world frame";return false;}
  if(!start(uint32_t(pixels))){quarantined_=true;return false;}
  if(cancelled(cancelSerial))return fail("DLSS 5 request was cancelled during startup");
  auto*h=static_cast<neuralframe::Header*>(view_);h->generation=generation;h->frame=frame;h->width=width;h->height=height;
  h->flags=neuralframe::DepthInverted|(resetHistory?neuralframe::ResetHistory:0);h->resultGeneration=h->resultFrame=0;h->backendCode=0;h->milliseconds=0;h->message[0]=0;
  auto*base=static_cast<uint8_t*>(view_);std::memcpy(base+layout_.color,color.data(),size_t(pixels)*4);std::memcpy(base+layout_.motion,motion.data(),size_t(pixels)*8);
  std::memcpy(base+layout_.depth,depth.data(),size_t(pixels)*4);std::memcpy(base+layout_.control,control.data(),size_t(pixels)*4);
  if(cancelled(cancelSerial))return fail("DLSS 5 request was cancelled before GPU submission");
  ResetEvent(complete_);h->status=uint32_t(neuralframe::Status::Pending);MemoryBarrier();
  if(!SetEvent(request_))return fail("DLSS 5 helper notification failed",true);
  HANDLE waits[]={complete_,child_};DWORD waited=WAIT_TIMEOUT;const DWORD started=GetTickCount(),budget=warmed_?1500:15000;
  do{if(cancelSerial_.load(std::memory_order_relaxed)!=cancelSerial)return fail("DLSS 5 request was cancelled");waited=WaitForMultipleObjects(2,waits,FALSE,25);}while(waited==WAIT_TIMEOUT&&DWORD(GetTickCount()-started)<budget);
  if(waited!=WAIT_OBJECT_0)return fail(waited==WAIT_TIMEOUT?"DLSS 5 helper timed out; normal rendering retained":"DLSS 5 helper exited; normal rendering retained",true);
  MemoryBarrier();
  if(!neuralframe::valid(*h,capacity_)||h->generation!=generation||h->frame!=frame||h->resultGeneration!=generation||h->resultFrame!=frame||h->width!=width||h->height!=height)return fail("DLSS 5 helper returned a mismatched frame",true);
  if(h->status!=uint32_t(neuralframe::Status::Completed)){std::string reason(h->message,std::find(h->message,h->message+sizeof h->message,char(0)));if(reason.empty())reason="DLSS 5 is unavailable";const auto status=neuralframe::Status(h->status);if(status==neuralframe::Status::Skipped||status==neuralframe::Status::InvalidInput){diagnostics_.reason=reason;diagnostics_.status=status;return false;}fail(reason.c_str(),true,status);return false;}
  output.assign(reinterpret_cast<const uint32_t*>(base+layout_.output),reinterpret_cast<const uint32_t*>(base+layout_.output)+size_t(pixels));
  for(size_t i=0;i<pixels;++i)if(control[i]<=0||(color[i]>>24)!=255)output[i]=color[i];else output[i]=(output[i]&0xffffff)|(color[i]&0xff000000);
  warmed_=true;diagnostics_.milliseconds=h->milliseconds;diagnostics_.used=true;diagnostics_.status=neuralframe::Status::Completed;++diagnostics_.frames;diagnostics_.reason="DLSS 5 neural rendering active";return true;
#else
  (void)color;(void)motion;(void)depth;(void)control;(void)width;(void)height;(void)generation;(void)frame;(void)resetHistory;(void)output;(void)expectedToken;
  diagnostics_.status=neuralframe::Status::Unsupported;diagnostics_.reason="DLSS 5 requires the 64-bit toolbox";return false;
#endif
 }
};
}
