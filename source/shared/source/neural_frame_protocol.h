#pragma once
// SPDX-License-Identifier: MIT
// SDK-independent, fixed-width IPC between the host and optional neural helper.
// No game pointers, SDK objects, or compiler-specific C++ objects cross this ABI.
#include <cstddef>
#include <cstdint>
#include <limits>
namespace neuralframe {
constexpr uint32_t Magic=0x354e4152,Version=1;
constexpr uint32_t MaximumDimension=8192,MaximumPixels=16u*1024u*1024u;
enum class Status:uint32_t {Idle,Pending,Processing,Completed,Unsupported,InvalidInput,BackendFailed,NoRuntime,ShuttingDown,Skipped};
enum Flag:uint32_t {ResetHistory=1,DepthInverted=2,Shutdown=4};
struct alignas(8) Header {
 uint32_t magic=Magic,version=Version,headerBytes=256,capacityPixels=0;
 uint64_t generation=0,frame=0;
 uint32_t width=0,height=0,flags=ResetHistory|DepthInverted,status=uint32_t(Status::Idle);
 uint64_t resultGeneration=0,resultFrame=0;
 uint32_t backendCode=0,reserved=0;
 double milliseconds=0;
 float intensity=.65f,toneStrength=.2f,structureStrength=.5f;
 uint32_t style=0;
 char message[160]{};
};
static_assert(sizeof(Header)==256&&alignof(Header)==8,"Neural IPC ABI");
static_assert(offsetof(Header,generation)==16&&offsetof(Header,resultGeneration)==48&&offsetof(Header,message)==96,"Neural IPC field offsets");
struct Layout {uint64_t color=0,motion=0,depth=0,control=0,output=0,bytes=0;};
inline bool layout(uint32_t capacity,Layout&out){
 out={};if(!capacity||capacity>MaximumPixels)return false;
 // Every plane starts at a cache-line boundary; capacity is immutable per child.
 auto aligned=[](uint64_t n){return (n+63)&~uint64_t(63);};
 out.color=sizeof(Header);out.motion=aligned(out.color+uint64_t(capacity)*4);
 out.depth=aligned(out.motion+uint64_t(capacity)*8);out.control=aligned(out.depth+uint64_t(capacity)*4);
 out.output=aligned(out.control+uint64_t(capacity)*4);out.bytes=aligned(out.output+uint64_t(capacity)*4);
 return out.bytes<=uint64_t(std::numeric_limits<size_t>::max());
}
inline bool dimensions(uint32_t width,uint32_t height,uint32_t capacity){return width>=3&&height>=3&&width<=MaximumDimension&&height<=MaximumDimension&&uint64_t(width)*height<=capacity&&capacity<=MaximumPixels;}
inline bool valid(const Header&h,uint32_t capacity){return h.magic==Magic&&h.version==Version&&h.headerBytes==sizeof(Header)&&h.capacityPixels==capacity&&dimensions(h.width,h.height,capacity)&&!(h.flags&~uint32_t(ResetHistory|DepthInverted|Shutdown))&&h.style<=2;}
}
