#pragma once
#include <algorithm>
#include <cstdint>

// Host-side, zero-RPM diagnostic aggregation. Runtime is NEVER asked to log
// a line or to scan a roster. Read already published IPC counters only.
// "failures" is a mixed diagnostic counter, NOT a count of missing frames.
namespace assist { namespace samplinglog {
struct Input {
 uint32_t pid=0,match=0,tick=0,serial=0,failures=0;
 uint32_t pages=0,reads=0,bytes=0;
 int32_t controlFrame=-1,sampleFrame=-1;
 uint32_t controlUs=0,scanUs=0,worldUs=0;
};
struct Summary {
 uint32_t periodMs=0,logicFrames=0,publications=0,failures=0;
 uint32_t lastPages=0,lastReads=0,lastBytes=0;
 int32_t lastFrameLag=-1,maxFrameLag=-1;
 uint32_t lastPublicationObservedAgoMs=0;
 uint32_t controlPeakUs=0,scanPeakUs=0,worldPeakUs=0;
 uint32_t controlOver16msSamples=0,observations=0;
 bool sampleEverSeen=false,starved=false;
};
class Monitor {
 Input first_{},last_{};
 uint32_t publicationSeenAt_=0,controlPeak_=0,scanPeak_=0,worldPeak_=0;
 uint32_t overBudget_=0,observations_=0;
 int32_t maxLag_=-1;
 bool active_=false,publicationSeen_=false;
 static int32_t lag(const Input&i){
  if(i.controlFrame<0||i.sampleFrame<0||i.sampleFrame>i.controlFrame)return -1;
  return i.controlFrame-i.sampleFrame;
 }
 void begin(const Input&i){
  first_=last_=i;active_=true;publicationSeen_=i.sampleFrame>=0&&i.serial!=0;
  publicationSeenAt_=i.tick;controlPeak_=scanPeak_=worldPeak_=0;
  overBudget_=observations_=0;maxLag_=-1;
 }
public:
 void reset(){active_=false;publicationSeen_=false;}
 bool observe(const Input&i,Summary&out,uint32_t intervalMs=5000){
  if(!i.pid||!i.match){reset();return false;}
  if(!active_||first_.pid!=i.pid||first_.match!=i.match||
     i.controlFrame<last_.controlFrame||uint32_t(i.tick-last_.tick)>60000){
   begin(i);return false;
  }
  const int32_t currentLag=lag(i);
  if(currentLag>=0)maxLag_=std::max(maxLag_,currentLag);
  if(i.serial!=last_.serial&&i.sampleFrame>=0){
   publicationSeen_=true;publicationSeenAt_=i.tick;
  }
  controlPeak_=std::max(controlPeak_,i.controlUs);
  scanPeak_=std::max(scanPeak_,i.scanUs);
  worldPeak_=std::max(worldPeak_,i.worldUs);
  if(i.controlUs>16667)++overBudget_;
  ++observations_;last_=i;
  if(uint32_t(i.tick-first_.tick)<intervalMs)return false;
  out={};
  out.periodMs=i.tick-first_.tick;
  out.logicFrames=i.controlFrame>=first_.controlFrame?uint32_t(i.controlFrame-first_.controlFrame):0;
  out.publications=i.serial-first_.serial;
  out.failures=i.failures-first_.failures;
  out.lastPages=i.pages;out.lastReads=i.reads;out.lastBytes=i.bytes;
  out.lastFrameLag=currentLag;out.maxFrameLag=maxLag_;
  out.lastPublicationObservedAgoMs=publicationSeen_?uint32_t(i.tick-publicationSeenAt_):out.periodMs;
  out.controlPeakUs=controlPeak_;out.scanPeakUs=scanPeak_;out.worldPeakUs=worldPeak_;
  out.controlOver16msSamples=overBudget_;out.observations=observations_;
  out.sampleEverSeen=publicationSeen_;
  // State is for diagnosis only; no command eligibility decisions use it.
  out.starved=!publicationSeen_||currentLag<0||currentLag>4||
              out.lastPublicationObservedAgoMs>250;
  begin(i);
  return true;
 }
};
}} // namespace assist::samplinglog
