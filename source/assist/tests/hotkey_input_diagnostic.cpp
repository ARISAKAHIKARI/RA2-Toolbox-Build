#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/hotkey_input_diagnostic.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <set>
#include <string>
#include <vector>
using namespace assist::hotkeydiagnostic;

static unsigned checks=0,scenarios=0;
static void check(bool value,const char*reason){
 ++checks;if(!value){std::fprintf(stderr,"FAIL check=%u: %s\n",checks,reason);std::exit(1);}
}
struct Group{int id;uint16_t key;};
using Keys=std::array<uint8_t,256>;
static constexpr uint16_t Load=uint16_t('L'|(6u<<8));
static const std::vector<Group> load{{1011,Load}};
static void observe(Monitor&m,const Keys&keys,const std::vector<Group>&groups,std::vector<Event>&events){
 m.observe(keys,groups,[&](Event e){events.push_back(e);});
}
static void total(const Monitor&m){check(m.chordMatches+m.mismatches==m.configuredPresses+m.heldOnSync,"all emitted chords are accounted for");}

int main(){
 {
  ++scenarios;Monitor m;
  const Gate gates[]={Gate::Ready,Gate::Disabled,Gate::NoGame,Gate::NotForeground,Gate::ConnectionNotReady,Gate::Chat,Gate::Stale,Gate::FormationOwned,Gate::MiddleOwned};
  std::set<std::wstring>names;
  for(unsigned i=0;i<GateCount;++i){check(m.begin(gates[i],7,9,2,100+i),"gate changes report immediately");check(m.perGate[i]==1,"gate-specific call accounting");names.insert(name(gates[i]));}
  check(names.size()==GateCount,"all gate names are distinct");check(m.calls==9&&m.blocked==8&&m.reads==0,"early-return calls stay observable without reads");
  check(!std::wcscmp(name(Gate::FormationOwned),L"formation-owned"),"stable textual gate name");
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;
  check(m.begin(Gate::Ready,1,3,4,0),"first context at tick zero reports");
  observe(m,keys,load,events);
  check(!m.begin(Gate::Ready,1,3,4,0),"same tick/context does not spam");
  check(!m.begin(Gate::Ready,1,3,4,4999),"heartbeat waits a full interval");
  check(m.begin(Gate::Ready,1,3,4,5000),"active service heartbeat reports");
  check(!m.begin(Gate::Ready,1,3,4,5001),"heartbeat is not per-call logging");
  check(events.empty()&&m.reads==1&&m.configuredPresses==0,"an idle sample has no key event");
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;m.begin(Gate::Ready,1,1,1,1);
  observe(m,keys,load,events);keys[17]=keys[18]=keys['L']=1;observe(m,keys,load,events);
  check(events.size()==1&&events[0].id==1011&&events[0].key==Load,"configured edge identifies its binding");
  check(events[0].kind==Kind::Press&&events[0].actualModifiers==6&&events[0].modifiersMatch,"Ctrl+Alt+L edge is a matching press");
  observe(m,keys,load,events);check(events.size()==1,"a held key does not repeat");
  keys['L']=0;observe(m,keys,load,events);check(events.size()==1,"release is bookkeeping only");
  keys[18]=0;keys['L']=1;observe(m,keys,load,events);
  check(events.size()==2&&!events.back().modifiersMatch&&events.back().actualModifiers==2,"a configured main key with wrong prefix is diagnosed");
  check(m.configuredPresses==2&&m.heldOnSync==0&&m.chordMatches==1&&m.mismatches==1,"press/mismatch counters");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;m.begin(Gate::Ready,1,1,1,1);
  keys[17]=keys[18]=keys['L']=1;observe(m,keys,load,events);
  check(events.size()==1&&events[0].kind==Kind::HeldOnSync,"first held sample is labelled as a baseline");
  check(m.configuredPresses==0&&m.heldOnSync==1,"initial hold is never a press");
  check(m.begin(Gate::NotForeground,1,1,1,2),"focus loss changes context");
  check(m.begin(Gate::Ready,1,1,1,3),"focus recovery changes context");observe(m,keys,load,events);
  check(events.size()==2&&events.back().kind==Kind::HeldOnSync&&m.configuredPresses==0,"held-through-focus-return remains synchronized");
  check(m.blocked==1&&m.calls==3&&m.reads==2,"no physical read is assumed while blocked");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;keys['L']=1;
  m.begin(Gate::Ready,1,1,1,1);observe(m,keys,load,events);
  check(m.begin(Gate::Ready,2,1,1,2),"PID change reports");observe(m,keys,load,events);
  check(m.begin(Gate::Ready,2,2,1,3),"match change reports");observe(m,keys,load,events);
  check(m.begin(Gate::Ready,2,2,2,4),"binding revision change reports");observe(m,keys,load,events);
  check(events.size()==4&&m.heldOnSync==4&&m.configuredPresses==0,"all identity changes rebaseline held keys");
  check(!m.begin(Gate::Ready,2,2,2,5),"unchanged identity does not report");observe(m,keys,load,events);check(events.size()==4,"unchanged hold remains quiet");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;
  std::vector<Group>aliases{{1011,Load},{2022,uint16_t(Load|0x8000)},{3033,uint16_t('L'|(2u<<8))}};
  m.begin(Gate::Ready,1,1,1,1);observe(m,keys,aliases,events);keys[17]=keys[18]=keys['L']=1;observe(m,keys,aliases,events);
  check(events.size()==2&&events[0].id==1011&&events[1].id==3033,"dedup is by main key plus expected prefix, not action or gesture");
  check(events[0].modifiersMatch&&!events[1].modifiersMatch,"distinct expected chords remain distinguishable");
  check(m.configuredPresses==2&&m.chordMatches==1&&m.mismatches==1,"deduplicated counters");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;m.begin(Gate::Ready,1,1,1,1);
  observe(m,keys,load,events);keys['L']=1;observe(m,keys,{},events);observe(m,keys,load,events);
  check(events.size()==1&&events[0].kind==Kind::HeldOnSync,"a removed/re-added held binding cannot become a press");
  check(m.configuredPresses==0&&m.heldOnSync==1,"implicit binding-set change remains conservative");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;m.begin(Gate::FormationOwned,1,1,1,1);
  observe(m,keys,load,events);keys[17]=keys[18]=keys['L']=1;observe(m,keys,load,events);
  check(events.size()==1&&events[0].kind==Kind::Press&&events[0].modifiersMatch,"explicit reads can diagnose formation-owned presses");
  m.begin(Gate::MiddleOwned,1,1,1,2);observe(m,keys,load,events);
  check(events.size()==2&&events.back().kind==Kind::HeldOnSync,"owner change rebaselines instead of fabricating an edge");
  check(m.blocked==2&&m.reads==3&&m.perGate[unsigned(Gate::FormationOwned)]==1&&m.perGate[unsigned(Gate::MiddleOwned)]==1,"blocked does not imply no explicit read");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;m.begin(Gate::Ready,1,1,1,1);observe(m,keys,load,events);
  keys[17]=keys[18]=keys[91]=keys['L']=1;observe(m,keys,load,events);
  check(events.size()==1&&events[0].actualModifiers==14&&!events[0].modifiersMatch,"Windows prefix is bit8 and prevents a chord match");
  keys[91]=0;observe(m,keys,load,events);check(events.size()==1,"releasing a modifier while main key stays down is not a new press");
  keys['L']=0;observe(m,keys,load,events);keys['L']=1;observe(m,keys,load,events);
  check(events.size()==2&&events.back().actualModifiers==6&&events.back().modifiersMatch,"next physical press can match without Windows prefix");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;std::vector<Group>bareAlt{{18,18}};
  m.begin(Gate::Ready,1,1,1,1);observe(m,keys,bareAlt,events);keys[18]=keys[17]=1;observe(m,keys,bareAlt,events);
  check(events.size()==1&&events[0].modifiersMatch&&events[0].actualModifiers==6,"bare modifier retains existing command matching semantics");
  keys[18]=0;observe(m,keys,bareAlt,events);keys[18]=keys[92]=1;observe(m,keys,bareAlt,events);
  check(events.size()==2&&!events.back().modifiersMatch&&events.back().actualModifiers==14,"Windows prefix is still diagnosed for a bare modifier");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;std::vector<Group>win{{91,91}};
  m.begin(Gate::Ready,1,1,1,1);observe(m,keys,win,events);keys[91]=1;observe(m,keys,win,events);
  check(events.size()==1&&events[0].actualModifiers==8&&events[0].modifiersMatch,"Windows main key is not its own prefix");
  keys[91]=0;observe(m,keys,win,events);keys[91]=keys[92]=1;observe(m,keys,win,events);
  check(events.size()==2&&!events.back().modifiersMatch,"other Windows key remains a prefix");total(m);
 }
 {
  ++scenarios;Monitor m;const uint32_t start=0xffffff00u;
  check(m.begin(Gate::Ready,1,1,1,start),"first report near wrap");
  check(!m.begin(Gate::Ready,1,1,1,start+4999u),"wrapped sub-interval is quiet");
  check(m.begin(Gate::Ready,1,1,1,start+5000u),"wrapped heartbeat is due");
  check(!m.begin(Gate::Ready,1,1,1,start+5001u),"wrapped clock does not leave a stuck deadline");
 }
 {
  ++scenarios;Monitor m;std::vector<Event>events;
  struct AuditedKeys{
   mutable std::set<unsigned>read;Keys values{};
   uint8_t operator[](unsigned vk)const{read.insert(vk);return values[vk];}
  }keys;
  keys.values['Z']=keys.values['Q']=1;
  m.begin(Gate::Ready,1,1,1,1);m.observe(keys,load,[&](Event e){events.push_back(e);});
  const std::set<unsigned>allowed{16,17,18,91,92,'L'};
  for(unsigned vk:keys.read)check(allowed.count(vk)!=0,"only configured main key and modifier slots are read");
  check(!keys.read.count('Z')&&!keys.read.count('Q')&&events.empty(),"unconfigured text keys are neither inspected nor emitted");
  check(m.reads==1&&m.configuredPresses==0&&m.heldOnSync==0,"unconfigured input leaves event counters untouched");total(m);
 }
 {
  ++scenarios;Monitor m;Keys keys{};std::vector<Event>events;
  std::vector<Group>empty{{1,0},{2,0x8000},{3,0x1000}};keys[0]=1;
  m.begin(Gate::Ready,1,1,1,1);observe(m,keys,empty,events);
  check(events.empty()&&m.configuredPresses==0&&m.heldOnSync==0,"unbound/gesture-only entries are ignored");
 }
 std::printf("PASS hotkey input diagnostics: %u scenarios, %u checks; pure observation, no Win32/game execution\n",scenarios,checks);
}
