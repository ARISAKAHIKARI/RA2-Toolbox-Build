#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iomanip>
#include <sstream>
#include <string>
namespace pipelineperf {
using Clock=std::chrono::steady_clock;
using Sink=void(*)(const std::wstring&);
inline double milliseconds(Clock::duration value){return std::chrono::duration<double,std::milli>(value).count();}
// Each instance belongs to one worker/UI thread. Aggregate in place; formatting
// and logging happen at most once per five seconds, including failed attempts.
class Window {
 struct Slot {const wchar_t*name=nullptr;uint64_t count=0;double sum=0,max=0;};
 std::array<Slot,16> slots_{};const wchar_t*name_;Clock::time_point since_=Clock::now();
public:
 Window(const wchar_t*name,std::initializer_list<const wchar_t*>stages):name_(name){size_t i=0;for(auto stage:stages)if(i<slots_.size())slots_[i++].name=stage;}
 void add(unsigned index,double ms){if(index>=slots_.size())return;auto&s=slots_[index];++s.count;s.sum+=ms;s.max=std::max(s.max,ms);}
 void report(Sink sink){auto now=Clock::now();if(!sink||now-since_<std::chrono::seconds(5))return;
  std::wostringstream text;text<<L"pipeline "<<name_<<L" window_ms="<<std::fixed<<std::setprecision(2)<<milliseconds(now-since_);
  for(auto&s:slots_)if(s.name){text<<L" "<<s.name<<L"[n="<<s.count<<L",mean_ms="<<(s.count?s.sum/s.count:0.)<<L",max_ms="<<s.max<<L"]";s.count=0;s.sum=s.max=0.;}
  since_=now;sink(text.str());
 }
};
class Scope {
 Window&window_;unsigned index_;Sink sink_;Clock::time_point started_=Clock::now();bool done_=false;
public:
 Scope(Window&window,unsigned index,Sink sink=nullptr):window_(window),index_(index),sink_(sink){}
 Scope(const Scope&)=delete;Scope&operator=(const Scope&)=delete;
 void finish(){if(done_)return;done_=true;window_.add(index_,milliseconds(Clock::now()-started_));if(sink_)window_.report(sink_);}
 ~Scope(){finish();}
};
}
