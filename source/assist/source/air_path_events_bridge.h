#pragma once
#include "native_planning_input_bridge.h"
// Native waypoint input without CanUseWaypoint/arrival patches. Capture only
// an aircraft-only selection; leave mixed groups and object clicks native.
namespace directair {
struct Actor {uint32_t pointer=0,id=0,token=0;};
static Actor actors[airevents::MaxActors];static unsigned count=0,pointCount=0,nodeAt=0,actorAt=0;
static uint32_t cells[airevents::MaxPoints],epoch=0,house=0,started=0,button=0,accepted=0;
static bool down=false,submitting=false;static NativePlanningInput planning;
static bool enabled(){return sh&&airpathpolicy::direct(get<int>(0xA8B238),(sh->flags&NativeAirWaypoints)!=0,sh->nativeAirNetwork);}
static bool pending(){return down||count||planning.active;}
static void report(int status,unsigned rejected=0){
 if(!sh)return;InterlockedIncrement(&sh->airEventSeq);MemoryBarrier();
 sh->airEventReport={match,status,pointCount,accepted,rejected};MemoryBarrier();InterlockedIncrement(&sh->airEventSeq);notifyHost();
}
static void reset(int status=airevents::Cancelled){
 if(count||down)report(accepted?airevents::Partial:status);
 down=submitting=false;count=pointCount=nodeAt=actorAt=button=accepted=0;planning.reset();
}
static bool valid(const Actor&a){return worldByID(a.id)==a.pointer&&own(a.pointer,lastHouse)&&kind(a.pointer)==0&&live(a.pointer)&&
 !get<bool>(a.pointer+0x81)&&!get<uint32_t>(a.pointer+0x11C)&&manualVersions.get(a.id)==a.token;}
static bool validate(const intent::EventOrder&o){
 if(!submitting)return true;
 return enabled()&&epoch==match&&house==lastHouse&&moOrdersContext()&&actorAt<count&&nodeAt<pointCount&&valid(actors[actorAt])&&
  o.domain==vanilla::Domain::MegaMission&&o.appendWaypoint&&o.player==get<int>(lastHouse+0x30)&&uint32_t(o.a)==actors[actorAt].id&&o.mission==2&&!o.b&&uint32_t(o.c)==cells[nodeAt]&&o.tagC==11;
}
static bool sameSelection(){
 Vec selected=vec(0xA8ECB8);if(selected.count!=int(count))return false;
 for(unsigned i=0;i<count;++i){if(!valid(actors[i]))return false;bool found=false;
  for(int j=0;j<selected.count;++j)found|=item(selected,j)==actors[i].pointer;if(!found)return false;}
 return true;
}
static bool held(){
 uint8_t keys[256]{};for(unsigned k=0;k<256;++k)keys[k]=(GetKeyState(int(k))&0x8000)!=0;
 auto bindings=nativekeys::load([](uint32_t at,void*out,size_t n){return read(at,out,n);});
 return bindings.held(nativekeys::Planning,keys);
}
static bool capture(MSG*msg){
 Vec selected=vec(0xA8ECB8);if(selected.count<=0||selected.count>int(airevents::MaxActors))return false;
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i),id=get<uint32_t>(p+0x10);
  if(!p||!worldObjects.contains(p)||!own(p,lastHouse)||kind(p)!=0||!live(p)||get<bool>(p+0x81)||get<uint32_t>(p+0x11C)||!manualVersions.trackable(id))return false;
 }
 uint8_t editor=0;if(!read(0x87F7E8+0x11B3,&editor,1)||editor!=1||!planning.capture(msg))return false;
 if(!planning.native){planning.reset();return false;}
 epoch=match;house=lastHouse;started=sampleClock(sh);pointCount=nodeAt=actorAt=accepted=0;
 count=unsigned(selected.count);
 for(unsigned i=0;i<count;++i){uint32_t p=item(selected,int(i));cancelNative(p,false);uint32_t id=get<uint32_t>(p+0x10);actors[i]={p,id,manualVersions.get(id)};}
 return true;
}
static bool message(MSG*msg){
 auto swallow=[&](){msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;};
 if(planning.message(msg,count||down))return true;
 if(planning.draining)return false;
 const auto edge=moorders::gestureMouseEdge(msg->message,uintptr_t(msg->wParam));
 const bool keyDown=msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN;
 if(count&&edge.down&&edge.button!=4&&edge.button!=6){reset();return false;}
 if(count&&keyDown&&(msg->wParam==VK_ESCAPE||((msg->wParam=='S'||msg->wParam=='D'||msg->wParam=='X')&&!planning.primary[unsigned(msg->wParam)&255]))){reset();return false;}
 if(pending()&&(!enabled()||!moOrdersContext(true)||epoch!=match||house!=lastHouse)){reset();return false;}
 if(down&&edge.up&&edge.button==button){
  down=false;int x=0,y=0,px=0,py=0;uint32_t hover=0;
  if(!moOrdersContext()||!sameSelection()||!formationPoint(msg,x,y,px,py,nullptr,&hover)||hover){reset(airevents::Invalid);return swallow();}
  const uint32_t cell=uint32_t(y/256*1000+x/256);
  if(pointCount&&cells[pointCount-1]==cell){report(airevents::Recorded);return swallow();}
  if(pointCount>=airevents::MaxPoints){report(airevents::Full,count);return swallow();}
  cells[pointCount++]=cell;started=sampleClock(sh);report(airevents::Recorded);return swallow();
 }
 if(!edge.down||(edge.button!=4&&edge.button!=6)||!enabled()||!moOrdersContext()||!held())return false;
 if(moOrdersPending()||formationView.active)return false;
 int x=0,y=0,px=0,py=0;uint32_t hover=0;if(!formationPoint(msg,x,y,px,py,nullptr,&hover)||hover){if(count)reset();return false;}
 indexWorld(get<int>(0xA8ED84));
 if(count?!sameSelection():!capture(msg)){report(airevents::Invalid);return false;}
 down=true;button=edge.button;return swallow();
}
static void tick(uint32_t currentHouse,int frame,DWORD now){
 if(planning.draining){planning.reset();return;}
 if(!pending())return;
 if(!enabled()||epoch!=match||house!=currentHouse||!moOrdersContext(true)){reset();return;}
 if(DWORD(now-started)>30000){reset();return;}
 if(!moOrdersContext())return;
 for(unsigned i=0;i<count;++i)if(!valid(actors[i])){reset();return;}
 while(nodeAt<pointCount&&outgoingRoom()>0){
  auto&a=actors[actorAt];uint32_t cell=cells[nodeAt];
  bool sent=false;runtimeunwind::run([&]{submitting=true;sent=nativeMission(currentHouse,a.id,2,0,cell,11,cell,11,taskarbiter::Owner::Generic,nullptr,-1,OrderAuthority::Local,true);},[&]{submitting=false;});
  if(!sent)break;
  ++accepted;if(!actorAt)inputFeedback(a.pointer,0,false); // once per clicked node, never on arrival
  if(++actorAt==count){actorAt=0;++nodeAt;}report(airevents::Sending);
 }
 if(!down&&nodeAt==pointCount&&planning.count&&!planning.physicalHeld()){
  // Nodes are already in the normal ordered SP/MP transport. The unchanged
  // real key-up now lets native UI enqueue its one House-wide PlanCommit.
  if(!planning.finish(currentHouse))return;
  report(airevents::Committed);count=pointCount=nodeAt=actorAt=button=accepted=0;
 }
}
}
static bool validateDirectAirSubmission(const intent::EventOrder&o){return directair::validate(o);}
static void resetDirectAir(){directair::reset();}
static bool directAirPending(){return directair::pending();}
