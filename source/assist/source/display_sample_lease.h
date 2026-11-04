#pragma once
#include <cstdint>
#include <utility>
namespace displaylease {
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
