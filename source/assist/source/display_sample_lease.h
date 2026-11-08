#pragma once
#include <cstdint>
#include <utility>
namespace displaylease {
// Host display only. A recently confirmed immutable world frame stays usable
// while simulation is paused. Do not renew its acquisition time or accept an
// old frame after confirmation is lost; command freshness remains separate.
template<class Frame> inline bool frameCurrent(const Frame& sample,uint32_t match,int frame,uint32_t now,bool confirmed,int maximum){
 return sample.match==match&&sample.tick&&sample.count>=0&&sample.count<=maximum&&
  (uint32_t(now-sample.tick)<250||(confirmed&&sample.frame==frame));
}
// Display pixels only: a failed observation never extends the deadline and
// must never authorize input. A successful empty value removes old content.
template<class Key,class Value>class Sample {
 Key key_{};Value value_{};uint32_t tick_=0;bool valid_=false;
public:
 void clear(){valid_=false;value_={};}
 const Value* get(const Key&key,uint32_t now,uint32_t grace=120){
  if(!valid_||!(key==key_)||uint32_t(now-tick_)>=grace){clear();return nullptr;}return &value_;
 }
 const Value* observe(const Key&key,bool complete,Value value,uint32_t now){
  if(complete){key_=key;value_=std::move(value);tick_=now;valid_=true;return &value_;}
  return get(key,now);
 }
};
}
