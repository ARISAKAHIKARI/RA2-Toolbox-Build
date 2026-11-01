#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
namespace overlayvisibility {
// Process-local presentation state. Never changes command/Hook ownership.
inline std::atomic<bool> shown{true};
inline std::atomic<uint32_t> revision{0};
inline bool visible(){return shown.load();}
inline void toggle(){bool before=shown.load();while(!shown.compare_exchange_weak(before,!before)){}revision.fetch_add(1);}
class Key {
 uint32_t binding_=0;bool down_=false,armed_=false;
public:
 bool sample(uint32_t binding,bool down,bool eligible,bool modifiersMatch){
  if(binding!=binding_){binding_=binding;armed_=false;down_=down;}
  if(!eligible||!(binding&255)){armed_=false;down_=down;return false;}
  // Re-entering the game or changing a binding while held never toggles.
  if(!armed_){down_=down;if(!down)armed_=true;return false;}
  const bool pressed=down&&!down_;down_=down;
  return pressed&&modifiersMatch;
 }
};
struct Context {
 uintptr_t window=0;uint32_t pid=0;
 bool operator==(const Context&o)const{return window==o.window&&pid==o.pid;}
};
// One edge detector is shared by the Assist and Statistics UI threads. A live
// owner renews its short lease; inactive modules cannot reset somebody else's
// key. Transfer requires release, so held keys never toggle twice on handoff.
class Dispatcher {
 Key key_;Context context_{};unsigned owner_=0;uint32_t tick_=0;
public:
 bool sample(unsigned source,const Context&context,uint32_t binding,bool down,bool eligible,bool modifiers,uint32_t now){
  eligible=eligible&&source&&context.window&&context.pid;
  if(!eligible){if(owner_==source){key_.sample(binding,down,false,modifiers);owner_=0;}return false;}
  if(owner_&&owner_!=source&&uint32_t(now-tick_)<100)return false;
  if(owner_!=source||!(context==context_)){key_.sample(binding,down,false,modifiers);owner_=source;context_=context;}
  tick_=now;return key_.sample(binding,down,true,modifiers);
 }
};
inline bool sampleKey(unsigned source,const Context&context,uint32_t binding,bool down,bool eligible,bool modifiers,uint32_t now){
 static std::mutex mutex;static Dispatcher dispatcher;
 std::lock_guard<std::mutex> lock(mutex);return dispatcher.sample(source,context,binding,down,eligible,modifiers,now);
}
}
