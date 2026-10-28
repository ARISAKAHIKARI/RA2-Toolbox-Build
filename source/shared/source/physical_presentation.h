#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Geometry-only policy for physical-pixel presentation. No Win32/game APIs.
// Windowed 1440p on a 4K monitor is NOT itself a 4K render target.
// Fullscreen exclusive cannot be proven reliably by HWND styles alone.
namespace physicalpresentation {
enum class Status : uint8_t {
 Ready, NativeResolution, WindowOnly, SuspiciousModeSwitch,
 AmbiguousDpi, InvalidGeometry, UnsupportedRatio
};
struct Observation {
 int sourceW=0,sourceH=0;
 int clientW=0,clientH=0;
 int monitorW=0,monitorH=0;       // desktop monitor rectangle, physical Win32 pixels
 int currentW=0,currentH=0;       // current display mode
 int registryW=0,registryH=0;     // configured/default display mode (heuristic)
 bool coversMonitor=false;
 bool captureFailed=false;
};
struct Result {
 Status status=Status::InvalidGeometry;
 int width=0,height=0;
 double scaleX=1,scaleY=1;
 bool permitted() const {
  return status==Status::Ready||status==Status::NativeResolution;
 }
 bool modeChangeSuspected() const {
  return status==Status::SuspiciousModeSwitch;
 }
};
inline Result classify(const Observation&o){
 Result d;d.width=o.clientW;d.height=o.clientH;
 if(o.captureFailed||o.clientW<320||o.clientH<200||
    o.clientW>8192||o.clientH>8192||
    uint64_t(o.clientW)*o.clientH>16u*1024*1024||
    o.sourceW<320||o.sourceH<200||o.sourceW>8192||o.sourceH>8192||
    o.monitorW<320||o.monitorH<200||o.currentW<320||o.currentH<200)
   return d;
 d.scaleX=double(o.clientW)/o.sourceW;
 d.scaleY=double(o.clientH)/o.sourceH;
 if(o.monitorW!=o.currentW||o.monitorH!=o.currentH){
  d.status=Status::AmbiguousDpi;return d;
 }
 // A real display-mode switch below the configured 4K mode while covering
 // the output is a conservative "exclusive suspected" signal, never proof.
 // The configured mode may differ for reasons unrelated to exclusivity.
 if(o.coversMonitor&&o.registryW>=o.currentW*1.1&&
    o.registryH>=o.currentH*1.1){
  d.status=Status::SuspiciousModeSwitch;return d;
 }
 if(!o.coversMonitor||o.clientW!=o.monitorW||o.clientH!=o.monitorH){
  d.status=Status::WindowOnly;return d;
 }
 if(std::abs(d.scaleX-d.scaleY)>0.025){
  d.status=Status::UnsupportedRatio;return d;
 }
 if(d.scaleX<0.99||d.scaleX>2.01||d.scaleY<0.99||d.scaleY>2.01){
  d.status=Status::UnsupportedRatio;return d;
 }
 d.status=(d.scaleX>1.01&&d.scaleY>1.01)?Status::Ready:Status::NativeResolution;
 return d;
}
}
