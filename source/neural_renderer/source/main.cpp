// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <string>
#include "vulkan_ngx.h"
static void message(neuralframe::Header&h,const std::string&text){std::memset(h.message,0,sizeof h.message);std::memcpy(h.message,text.data(),std::min(text.size(),sizeof h.message-1));}
static HANDLE handle(const wchar_t*text){wchar_t*end=nullptr;auto value=std::wcstoull(text,&end,10);return value&&end&&!*end?reinterpret_cast<HANDLE>(uintptr_t(value)):nullptr;}
int wmain(int argc,wchar_t**argv){
 if(argc==2&&std::wstring(argv[1])==L"--probe"){neuralbackend::VulkanNGX backend;bool ready=backend.initialize();std::printf("DLSS5 status=%u available=%u reason=%s\n",unsigned(backend.status()),unsigned(ready),backend.reason().c_str());return ready?0:2;}
 if(argc!=6||std::wstring(argv[1])!=L"--ipc")return 1;HANDLE mapping=handle(argv[2]),request=handle(argv[3]),complete=handle(argv[4]),parent=handle(argv[5]);if(!mapping||!request||!complete||!parent)return 1;
 auto*head=static_cast<neuralframe::Header*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(neuralframe::Header)));if(!head)return 1;uint32_t capacity=head->capacityPixels;bool initial=head->magic==neuralframe::Magic&&head->version==neuralframe::Version&&head->headerBytes==sizeof(neuralframe::Header);UnmapViewOfFile(head);neuralframe::Layout layout;if(!initial||!neuralframe::layout(capacity,layout))return 1;
 auto*view=static_cast<uint8_t*>(MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,size_t(layout.bytes)));if(!view)return 1;head=reinterpret_cast<neuralframe::Header*>(view);neuralbackend::VulkanNGX backend;HANDLE waits[]={parent,request};
 for(;;){DWORD wait=WaitForMultipleObjects(2,waits,FALSE,INFINITE);if(wait!=WAIT_OBJECT_0+1)break;MemoryBarrier();const neuralframe::Header input=*head;if(input.flags&neuralframe::Shutdown)break;const auto start=std::chrono::steady_clock::now();
  const auto*color=reinterpret_cast<const uint32_t*>(view+layout.color);const auto*motion=reinterpret_cast<const float*>(view+layout.motion);const auto*depth=reinterpret_cast<const float*>(view+layout.depth);const auto*control=reinterpret_cast<const float*>(view+layout.control);auto*out=reinterpret_cast<uint32_t*>(view+layout.output);
  bool valid=neuralframe::valid(input,capacity)&&input.status==uint32_t(neuralframe::Status::Pending)&&std::isfinite(input.intensity)&&input.intensity>=0&&input.intensity<=1&&std::isfinite(input.toneStrength)&&input.toneStrength>=0&&input.toneStrength<=1&&std::isfinite(input.structureStrength)&&input.structureStrength>=0&&input.structureStrength<=1;
  size_t pixels=size_t(input.width)*input.height;bool any=false;if(valid)for(size_t i=0;i<pixels;++i){if(!std::isfinite(motion[i*2])||!std::isfinite(motion[i*2+1])||std::abs(motion[i*2])>32768||std::abs(motion[i*2+1])>32768||!std::isfinite(depth[i])||depth[i]<0||depth[i]>1||!std::isfinite(control[i])||control[i]<0||control[i]>1){valid=false;break;}any|=control[i]>0&&(color[i]>>24)==255;}
  head->status=uint32_t(neuralframe::Status::Processing);bool success=valid&&any&&backend.process(input,color,motion,depth,control,out);
  head->resultGeneration=input.generation;head->resultFrame=input.frame;head->backendCode=valid&&any?backend.code():0;head->milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();head->status=uint32_t(!valid?neuralframe::Status::InvalidInput:!any?neuralframe::Status::Skipped:success?neuralframe::Status::Completed:backend.status());message(*head,!valid?"DLSS 5 requires complete, finite world guides":!any?"DLSS 5 skipped this fully protected world frame":backend.reason());MemoryBarrier();if(!SetEvent(complete))break;if(backend.broken())break;
 }
 UnmapViewOfFile(view);for(auto h:{mapping,request,complete,parent})CloseHandle(h);return 0;
}
