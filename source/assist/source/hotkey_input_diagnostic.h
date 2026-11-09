#pragma once
#include <array>
#include <cstdint>

namespace assist { namespace hotkeydiagnostic {

// Observation only. These values explain Host admission; they never grant a
// command, select an HWND, register a key, or alter the production Registry.
enum class Gate : uint8_t {
 Ready,Disabled,NoGame,NotForeground,ConnectionNotReady,Chat,Stale,
 FormationOwned,MiddleOwned
};
constexpr unsigned GateCount=9;
inline const wchar_t* name(Gate gate){
 switch(gate){
 case Gate::Ready:return L"ready";
 case Gate::Disabled:return L"disabled";
 case Gate::NoGame:return L"no-game";
 case Gate::NotForeground:return L"not-foreground";
 case Gate::ConnectionNotReady:return L"connection-not-ready";
 case Gate::Chat:return L"chat";
 case Gate::Stale:return L"stale";
 case Gate::FormationOwned:return L"formation-owned";
 case Gate::MiddleOwned:return L"middle-owned";
 }
 return L"unknown";
}

enum class Kind : uint8_t {Press,HeldOnSync};
struct Event {
 int id=0;
 uint16_t key=0;
 // HOTKEYF Shift/Ctrl/Alt bits plus bit8 for either Windows key. The main
 // key is included here; modifiersMatch accounts for modifier-only bindings.
 uint8_t actualModifiers=0;
 Kind kind=Kind::Press;
 bool modifiersMatch=false;
};

class Monitor {
 // State is per physical main key AND expected prefix. Multiple gesture
 // variants/actions sharing that chord produce one diagnostic observation.
 // 0=not in the last observed binding set, 1=up, 2=down.
 std::array<uint8_t,2048> previous_{};
 bool contextKnown_=false,synchronize_=true,reported_=false;
 uint32_t reportedAt_=0;
 uint64_t reportedCalls_=0,reportedReads_=0,reportedBlocked_=0;

 template<class Keys>static uint8_t modifiers(const Keys&keys){
  return uint8_t((keys[16]?1:0)|(keys[17]?2:0)|(keys[18]?4:0)|
                 ((keys[91]||keys[92])?8:0));
 }
 template<class Keys>static bool chordMatchesNow(const Keys&keys,uint16_t chord,uint8_t actual){
  const unsigned vk=chord&255u,expected=(chord>>8)&7u;
  // A Windows key used as the main key is not its own prefix. The other
  // Windows key still is; a single bit in actualModifiers cannot express it.
  if((keys[91]&&vk!=91)||(keys[92]&&vk!=92))return false;
  // Preserve the existing modifier-only command-binding semantics.
  if(chord>=16&&chord<=18)return true;
  unsigned compared=7u;
  if(vk>=16&&vk<=18)compared&=~(1u<<(vk-16));
  return (unsigned(actual)&compared)==(expected&compared);
 }

public:
 uint64_t calls=0,reads=0,blocked=0,configuredPresses=0,heldOnSync=0;
 uint64_t chordMatches=0,mismatches=0;
 std::array<uint64_t,GateCount> perGate{};
 Gate gate=Gate::NoGame;
 uint32_t pid=0,match=0,bindingRevision=0;
 static constexpr uint32_t HeartbeatMs=5000;

 // Called once per diagnostic service entry, before any early return. A
 // changed context is reported immediately; steady activity is reported at
 // most once per five seconds. Explicit booleans make tick zero and wrapping
 // DWORD clocks ordinary timestamps, never sentinel deadlines.
 bool begin(Gate next,uint32_t nextPid,uint32_t nextMatch,uint32_t nextBindings,uint32_t now){
  ++calls;const unsigned index=unsigned(next);
  if(index<GateCount)++perGate[index];
  if(next!=Gate::Ready)++blocked;
  const bool changed=!contextKnown_||gate!=next||pid!=nextPid||match!=nextMatch||bindingRevision!=nextBindings;
  if(changed){
   gate=next;pid=nextPid;match=nextMatch;bindingRevision=nextBindings;
   contextKnown_=true;synchronize_=true;
  }
  const bool activity=calls!=reportedCalls_||reads!=reportedReads_||blocked!=reportedBlocked_;
  if(!changed&&reported_&&(!activity||uint32_t(now-reportedAt_)<HeartbeatMs))return false;
  reported_=true;reportedAt_=now;
  reportedCalls_=calls;reportedReads_=reads;reportedBlocked_=blocked;
  return true;
 }

 // The caller decides whether physical input may be read. In particular it
 // can explicitly observe FormationOwned/MiddleOwned to explain a rejected
 // configured key. A changed gate/epoch/binding set first establishes a
 // baseline: already-held keys are labelled HeldOnSync, never a new Press.
 // Only configured main keys and five modifier-state slots are inspected;
 // no arbitrary text keys, character translation, or full keyboard copy.
 // Groups need id/key members; emit receives Event by value. Neither input
 // keys nor groups are modified. Event counts are per deduplicated chord.
 template<class Keys,class Groups,class Emit>void observe(const Keys&keys,const Groups&groups,Emit emit){
  ++reads;std::array<uint8_t,2048> observed{};
  const uint8_t actual=modifiers(keys);
  for(const auto&group:groups){
   const uint16_t key=uint16_t(group.key),chord=uint16_t(key&0x7ffu);
   const unsigned vk=chord&255u;
   if(!vk||observed[chord])continue;
   observed[chord]=1;
   const bool down=keys[vk]!=0;
   const bool baseline=synchronize_||!previous_[chord];
   const bool pressed=down&&previous_[chord]==1;
   previous_[chord]=down?2:1;
   if(!down||(!baseline&&!pressed))continue;
   const bool matched=chordMatchesNow(keys,chord,actual);
   if(baseline)++heldOnSync;else ++configuredPresses;
   if(matched)++chordMatches;else ++mismatches;
   emit(Event{int(group.id),key,actual,baseline?Kind::HeldOnSync:Kind::Press,matched});
  }
  // Removing and later restoring a binding cannot turn a key which stayed
  // held into a fresh press, even if the caller forgot to bump its revision.
  for(unsigned chord=0;chord<previous_.size();++chord)if(!observed[chord])previous_[chord]=0;
  synchronize_=false;
 }
};

}} // assist::hotkeydiagnostic
