#pragma once
#include <cstdint>
namespace hooktest {
enum class Phase { Idle, Installing, Observing, Question, Restoring, Blocked, Stopping, Stopped, Complete };
struct Sequence {
 Phase phase=Phase::Idle;uint64_t since=0;unsigned index=0;bool disconnected=false;
 static constexpr uint64_t Interval=30000;
 void start(unsigned step,uint64_t now){index=step;since=now;disconnected=false;phase=Phase::Installing;}
 void installed(uint64_t now){if(phase==Phase::Installing){since=now;phase=Phase::Observing;}}
 void pause(uint64_t elapsed){if(phase==Phase::Observing||phase==Phase::Question)since+=elapsed;}
 bool tick(uint64_t now){if(phase==Phase::Observing&&now>=since&&now-since>=Interval){phase=Phase::Question;return true;}return false;}
 bool normal(){if((phase!=Phase::Observing&&phase!=Phase::Question)||disconnected)return false;phase=Phase::Restoring;return true;}
 bool skip(bool restorationBlocked=false){
  if(disconnected||restorationBlocked||(phase!=Phase::Observing&&phase!=Phase::Question&&phase!=Phase::Blocked))return false;
  phase=Phase::Restoring;return true;
 }
 void stop(bool oos=false){disconnected|=oos;phase=Phase::Stopping;}
 bool restored(bool verified,bool more,uint64_t now){if(!verified||disconnected||phase!=Phase::Restoring)return false;if(more){++index;since=now;phase=Phase::Installing;}else phase=Phase::Complete;return true;}
};
}
