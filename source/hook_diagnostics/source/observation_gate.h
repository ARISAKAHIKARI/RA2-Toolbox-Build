#pragma once
#include <cstdint>
namespace hooktest {
enum class Verification { Ready, Waiting, Failed };
struct ObservationGate {
 bool background=false,recovering=false,mismatch=false;
 uint64_t mismatchAt=0,lastVerified=0;
 bool focus(bool foreground){
  if(!foreground){background=true;recovering=false;mismatch=false;return false;}
  if(background){background=false;recovering=true;mismatch=false;}
  return true;
 }
 Verification verify(bool complete,uint64_t now){
  if(complete){mismatch=false;recovering=false;lastVerified=now;return Verification::Ready;}
  if(!mismatch||now<mismatchAt){mismatch=true;mismatchAt=now;}
  // A seqlock snapshot can be busy for a single timer poll. After focus
  // returns, allow the game thread to resume publishing coherent snapshots.
  return now-mismatchAt>=(recovering?5000u:1000u)?Verification::Failed:Verification::Waiting;
 }
};
}
