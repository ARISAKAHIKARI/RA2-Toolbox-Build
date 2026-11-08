#pragma once
// Test I/O boundary for legacy Runtime fixtures. The read-set, capture graph
// and scope bodies are production code; only the process bytes are synthetic.
#include "../../../../shared/source/world_frame_capture.h"
static worldframe::Snapshot worldFrame;
static bool worldFrameReady=false,sampleReadComplete=true,fixtureCaptureAttempted=false;
static unsigned sampleReadDepth=0;
static std::array<uint64_t,8192> runtimeDemandBits{};
static std::vector<uint32_t> runtimeWorldDemands;
static worldframe::Key fixtureLastAttempt{};
static uint64_t fixtureCaptureCount=0,fixtureCaptureReads=0,fixtureCaptureBytes=0,fixtureSampleReads=0,fixtureLiveReads=0,fixtureCaptureCellReads=0,fixtureSampleCellReads=0;
static bool readFixtureLive(uint32_t,void*,size_t);
#include "fixture_sample_scope_production.h"
static bool read(uint32_t at,void*out,size_t n){
 if(sampleReadDepth){++fixtureSampleReads;if(at>=CellBase&&uint64_t(at)+n<=uint64_t(CellBase)+512*512*0x200ull)++fixtureSampleCellReads;return readWorldSample(at,out,n);}
 ++fixtureLiveReads;return readFixtureLive(at,out,n);
}
static void fixtureResetWorld(){
 assert(!sampleReadDepth);worldFrameReady=fixtureCaptureAttempted=false;sampleReadComplete=true;
 runtimeWorldDemands.clear();runtimeDemandBits.fill(0);
 fixtureCaptureCount=fixtureCaptureReads=fixtureCaptureBytes=fixtureSampleReads=fixtureLiveReads=fixtureCaptureCellReads=fixtureSampleCellReads=0;
}
static bool fixtureCaptureWorld(uint32_t now){
 assert(!sampleReadDepth);worldframe::Key key{};key.match=match;key.frame=snap.frame;key.tick=now;key.house=lastHouse;key.session=snap.session;
 readFixtureLive(0xA8B230,&key.scenario,4);readFixtureLive(0x87F924,&key.cells,4);readFixtureLive(0x87F928,&key.cellCount,4);
 if(fixtureCaptureAttempted&&fixtureLastAttempt.sameFrame(key))return worldFrameReady;
 fixtureLastAttempt=key;fixtureCaptureAttempted=true;worldFrameReady=false;
 auto demands=std::move(runtimeWorldDemands);runtimeWorldDemands.clear();runtimeDemandBits.fill(0);
 worldFrame.reset(key);worldframe::CaptureGraph<decltype(&readFixtureLive)> capture(worldFrame,&readFixtureLive);
 const unsigned beforeCellReads=cellReads;const bool complete=capture.capture(worldframe::Base|worldframe::Ore|worldframe::Combat,demands);fixtureCaptureCellReads+=cellReads-beforeCellReads;
 ++fixtureCaptureCount;fixtureCaptureReads+=worldFrame.counters.calls;fixtureCaptureBytes+=worldFrame.counters.bytes;
 worldFrameReady=worldFrame.seal()&&complete;return worldFrameReady;
}
// Original fixtures expose a sparse logical map. Support the actual producer's
// page reads without allocating a 128 MiB array of mostly empty Cell objects.
// Returns -1 outside the two synthetic map spans, otherwise a successful read.
static int fixtureReadMap(uint32_t at,void*out,size_t n){
 if(at>=Cells&&uint64_t(at)+n<=uint64_t(Cells)+512*512*4){
  auto*dest=static_cast<uint8_t*>(out);while(n){const uint32_t index=(at-Cells)/4,offset=(at-Cells)%4,id=(index/512)*1000+index%512;
   const uint32_t p=nullCells.count(id)?0:CellBase+index*0x200;const size_t take=std::min<size_t>(n,4-offset);
   memcpy(dest,reinterpret_cast<const uint8_t*>(&p)+offset,take);dest+=take;at+=uint32_t(take);n-=take;
  }return 1;
 }
 if(at>=CellBase&&uint64_t(at)+n<=uint64_t(CellBase)+512*512*0x200ull){
  ++cellReads;auto*dest=static_cast<uint8_t*>(out);while(n){const uint32_t index=(at-CellBase)/0x200,offset=(at-CellBase)%0x200;uint8_t data[0x200]{};
   const int16_t xy[]={int16_t(index%512),int16_t(index/512)};memcpy(data+0x24,xy,4);Cell c{};auto found=cells.find(uint32_t(xy[1]*1000+xy[0]));if(found!=cells.end())c=found->second;
   memcpy(data+0x44,&c.overlay,4);memcpy(data+0xEC,&c.land,4);data[0x11B]=uint8_t(c.level);data[0x11C]=c.slope;data[0x11E]=c.amount;memcpy(data+0x124,&c.occupation,4);
   const size_t take=std::min<size_t>(n,0x200-offset);memcpy(dest,data+offset,take);dest+=take;at+=uint32_t(take);n-=take;
  }return 1;
 }
 return -1;
}
