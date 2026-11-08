#include <deque>
#include "native_viewport_host.h"
namespace airway {
struct Point {uint32_t cell=0;int z=0;};
struct Job {uint32_t id=0,pointer=0,token=0,match=0;std::deque<Point> points;int stage=0,attempts=0,x=0,y=0,z=0,previousX=0,previousY=0;bool selected=false,observedMove=false,previousValid=false;DWORD sent=0,progress=0,firstIssued=0;int64_t nearest=INT64_MAX;};
struct Line {int x,y,tx,ty;};
static std::vector<Job> jobs;static size_t cursor=0;static bool ended=false;static DWORD sampled=0;
static bool native(){return airpathpolicy::native(snap.session,extra.nativeAirWaypoints&&!extra.airPathKey,extra.nativeAirNetwork);}
static bool active(){return airpathpolicy::events(snap.session,extra.airWaypoints||(extra.nativeAirWaypoints&&extra.airPathKey),extra.nativeAirWaypoints&&!extra.airPathKey,extra.nativeAirNetwork);}
template<class K>static bool held(const K&keys){return extra.airPathKey?pointerinput::matchesHold(keys,extra.airPathKey,pointerinput::Enhanced):nativekeyhost::current().held(nativekeys::Planning,keys);}
static bool held(){uint8_t keys[256]{};nativekeyhost::down(keys);return held(keys);}
static void reset(){for(int n=0;n<commandQueue.size;)if(commandQueue.slots[n].command==AirPathMove)commandQueue.discard(n);else ++n;jobs.clear();cursor=0;ended=false;sampled=0;}
template<class T>static T get(uint32_t at,T fallback={}){T v{};return externalReader.readFresh(at,&v,sizeof v)?v:fallback;}
static void cancel(uint32_t id){for(size_t n=0;n<jobs.size();)if(jobs[n].id==id)jobs.erase(jobs.begin()+n);else ++n;for(int n=0;n<commandQueue.size;)if(commandQueue.slots[n].command==AirPathMove&&uint32_t(commandQueue.slots[n].arg)==id)commandQueue.discard(n);else ++n;}
static bool selectedAircraft(){uint32_t items=get<uint32_t>(0xA8ECB8+4);int n=get<int>(0xA8ECB8+16,-1);if(items<0x10000||n<0||n>4096)return false;for(int i=0;i<n;++i)if(get<uint32_t>(get<uint32_t>(items+i*4))==0x7E22A4)return true;return false;}
static void manual(const gameinput::PointerPress&e){
 if(!held())ended=false;
 const bool editing=held(e.keys),stop=e.operation==pointerinput::Cancel&&(e.keys['S']||e.keys['D']||e.keys['X']);
 const bool command=e.operation==4&&!editing;
 const bool attack=(e.operation==7||e.operation==6)&&e.keys[VK_CONTROL]&&!editing;
 if(!stop&&!command&&!attack)return; // selection clicks never cancel routes
 uint32_t items=get<uint32_t>(0xA8ECB8+4);int count=get<int>(0xA8ECB8+16,-1);if(items<0x10000||count<0||count>4096)return;
 for(int n=0;n<count;++n){uint32_t p=get<uint32_t>(items+n*4);if(get<uint32_t>(p)==0x7E22A4)cancel(get<uint32_t>(p+0x10));}
}
static void capture(const pointerinput::Intent&i,const pointerinput::Mailbox&box){
 if(!active())return;
 if(box.result<0){
  critical(L"air-path capture rejected result="+std::to_wstring(box.result)+L" match="+std::to_wstring(i.match)+L" sources="+std::to_wstring(i.count));
  notify(box.result==-7?L"飞机路径点未记录：请求已过期或对局已改变，请重新点击。":L"飞机路径点未记录：当前目标或游戏状态不支持。");return;
 }
 if(box.resolvedTarget){for(int n=0;n<i.count;++n)cancel(i.sources[n].id);ended=true;notify(L"攻击 / 对象命令已结束飞机路径点设置。");return;}
 if(!box.resolvedCell||ended)return;int added=0;
 for(int n=0;n<i.count;++n){if(!(box.sourceAccepted[n/32]&(1u<<(n%32))))continue;uint32_t id=i.sources[n].id;auto found=std::find_if(jobs.begin(),jobs.end(),[&](const Job&j){return j.id==id;});
  if(found==jobs.end()){if(jobs.size()>=512)continue;jobs.push_back({});found=jobs.end()-1;found->id=id;found->pointer=i.sources[n].pointer;found->match=i.match;}
  if(found->match!=i.match){*found={};found->id=id;found->pointer=i.sources[n].pointer;found->match=i.match;}
  if(found->token!=box.sourceTokens[n]){found->token=box.sourceTokens[n];found->stage=0;found->attempts=0;found->firstIssued=found->progress=0;found->nearest=INT64_MAX;found->observedMove=found->previousValid=false;}
  if(found->points.size()>=64)continue;if(found->points.empty()||found->points.back().cell!=box.resolvedCell){found->points.push_back({box.resolvedCell,box.resolvedZ});++added;}
 }
 if(!box.accepted){notify(L"飞机路径点未记录：所选单位中没有可用的己方飞机。");return;}
 critical(L"air-path recorded aircraft="+std::to_wstring(added)+L" cell="+std::to_wstring(box.resolvedCell)+L" match="+std::to_wstring(i.match));
 ++paintRevision;notify(L"飞机路径点已记录 "+std::to_wstring(added)+L" 架；松开路径点按键后执行，攻击命令结束设置。");
}
static void result(int status,int count,uint32_t id,uint32_t token){for(auto&j:jobs)if(j.id==id&&j.token==token&&j.stage==1){
 if(status>0&&count>0){j.stage=2;j.sent=GetTickCount();if(!j.progress)j.progress=j.sent;j.observedMove=false;}
 else if((status==-6||status==-7||status==-3)&&DWORD(GetTickCount()-j.firstIssued)<15000){j.stage=0;j.sent=GetTickCount()+150;}
 else {cancel(id);notify(L"飞机路径任务已失效，已释放给原生命令。");}return;}}
static bool crossed(const Job&j,int gx,int gy){
 // Host samples may straddle a waypoint at high game speed. Use the swept
 // ground segment, not the plane's elevated on-screen sprite or one sample.
 if(!j.previousValid)return false;
 const double dx=double(j.x)-j.previousX,dy=double(j.y)-j.previousY,length=dx*dx+dy*dy;
 if(length<=0||length>4096.0*4096.0)return false;
 const double t=std::clamp(((gx-j.previousX)*dx+(gy-j.previousY)*dy)/length,0.0,1.0);
 const double x=j.previousX+t*dx-gx,y=j.previousY+t*dy-gy;
 return x*x+y*y<=256.0*256.0;
}
enum class AircraftState { Unknown,Retired,Current };
static AircraftState currentAircraft(const Job&j,bool*selected=nullptr){
 if(j.match!=commandMatch())return AircraftState::Retired;
 uint32_t id=0,after=0,kind=0,owner=0,currentOwner=0;int hp=0;uint8_t retired=0,selection=0;
 auto read=[](uint32_t at,void*out,size_t size){return externalReader.readLive(at,out,size);};
 if(!read(j.pointer+0x10,&id,4)||!read(j.pointer,&kind,4)||!read(j.pointer+0x21C,&owner,4)||
    !read(0xA83D4C,&currentOwner,4)||!read(j.pointer+0x6C,&hp,4)||!read(j.pointer+0x6CA,&retired,1))return AircraftState::Unknown;
 if(id!=j.id||kind!=0x7E22A4||currentOwner<0x10000||owner!=currentOwner||hp<=0||retired)return AircraftState::Retired;
 if(selected&&!read(j.pointer+0x83,&selection,1))return AircraftState::Unknown;
 if(!read(j.pointer+0x10,&after,4))return AircraftState::Unknown;if(after!=j.id)return AircraftState::Retired;
 if(selected)*selected=selection!=0;
 return AircraftState::Current;
}
static void service(){
 DWORD now=GetTickCount();if(!held())ended=false;
 if(!active()||!enabled){if(!jobs.empty())reset();return;}
 if(!shared||!commandFresh(now)||snap.chat)return;
 if(jobs.empty())return;
 struct Observation {uint32_t id,pointer,token;int x,y,z,mission,dx,dy;uint32_t destination;};
 std::vector<Observation> observations;size_t nextCursor=cursor;
 {
  externalReader.setWorldDemand(worldframe::Base|worldframe::Combat);
  external::WorldFrameScope source(externalReader,commandMatch());if(!source)return;
  for(size_t n=0;n<std::min<size_t>(jobs.size(),16);++n){if(nextCursor>=jobs.size())nextCursor=0;const auto&j=jobs[nextCursor++];const uint32_t p=j.pointer;
   // A route accepted after this frame was sealed may refer to a new object
   // occupying the same address. Historical facts cannot retire that route.
   if(get<uint32_t>(p+0x10)!=j.id||get<uint32_t>(p)!=0x7E22A4)continue;
   Observation v{j.id,p,j.token,get<int>(p+0x9C),get<int>(p+0xA0),get<int>(p+0xA4),get<int>(p+0xAC,-1),0,0,get<uint32_t>(p+0x5A4)};
   if(v.mission==2&&v.destination>=0x10000){v.dx=get<int16_t>(v.destination+0x24);v.dy=get<int16_t>(v.destination+0x26);}
   observations.push_back(v);
  }
  if(!source.complete())return;
 }
 cursor=nextCursor;int sent=0;
 // Preserve the route worker's bounded continuation: arrival may pop a point
 // and submit the next one in this service call, reusing the same raw facts.
 for(size_t handled=0;handled<16&&!jobs.empty()&&!observations.empty();++handled){const auto&sample=observations[handled%observations.size()];auto found=std::find_if(jobs.begin(),jobs.end(),[&](const Job&j){return j.id==sample.id&&j.pointer==sample.pointer&&j.token==sample.token;});if(found==jobs.end())continue;auto&j=*found;
  // The route observation comes from one sealed world. Retirement and final
  // command identity checks below deliberately remain current live reads.
  bool selected=false;const auto state=currentAircraft(j,&selected);if(state==AircraftState::Unknown)continue;
  if(state==AircraftState::Retired){uint32_t id=j.id;cancel(id);continue;}
  j.x=sample.x;j.y=sample.y;j.z=sample.z;j.selected=selected;
  int mission=sample.mission;
  // Native automatic acquisition/return-to-base is not a player's cancel.
  // Explicit right-click/attack/stop is handled by manual() and token checks.
  if(j.points.empty()){uint32_t id=j.id;cancel(id);continue;}
  if(held())continue;
  const auto goal=j.points.front();const int gx=int(goal.cell%1000)*256+128,gy=int(goal.cell/1000)*256+128;
  int64_t dx=int64_t(j.x)-gx,dy=int64_t(j.y)-gy,distance=dx*dx+dy*dy;
  bool going=mission==2&&sample.destination>=0x10000&&sample.dx==int(goal.cell%1000)&&sample.dy==int(goal.cell/1000);
  if(j.stage==2){
   j.observedMove|=going;
   if(distance<=256*256||(j.observedMove&&crossed(j,gx,gy))){j.points.pop_front();j.stage=0;j.attempts=0;j.firstIssued=j.progress=0;j.observedMove=false;j.previousValid=false;j.nearest=INT64_MAX;++paintRevision;continue;}
   if(distance+32*32<j.nearest){j.nearest=distance;j.progress=now;}
   if(uint32_t(now-j.progress)>5000||(!going&&uint32_t(now-j.sent)>(j.observedMove?250u:1500u))){
    if(uint32_t(now-j.firstIssued)>30000&&uint32_t(now-j.progress)>5000){uint32_t id=j.id;cancel(id);notify(L"飞机未能到达路径点，已停止重复下令。");continue;}
    j.stage=0;j.sent=now+150;
   }
  }
  j.previousX=j.x;j.previousY=j.y;j.previousValid=true;
  if(j.stage==1&&uint32_t(now-j.sent)>15000){uint32_t id=j.id;cancel(id);continue;}
  if(j.stage==0&&sent<4&&int32_t(now-j.sent)>=0){
   const auto finalState=currentAircraft(j);if(finalState==AircraftState::Unknown)continue;
   if(finalState==AircraftState::Retired){uint32_t id=j.id;cancel(id);continue;}
   if(commandFresh(GetTickCount())&&!snap.chat&&queueCommand(AirPathMove,int(j.id),int(goal.cell),j.match,0,0,j.token)){j.stage=1;j.sent=now;if(!j.firstIssued)j.firstIssued=now;++j.attempts;++sent;}
  }
 }
}
static std::vector<Line> lines(){std::vector<Line> out;if(!active())return out;nativeviewhost::Transform transform;if(!nativeviewhost::sampleTransform(shared,transform))return out;external::Projection projection{snap.cameraX,snap.cameraY,transform.active?transform.logical.w:snap.width,transform.active?transform.logical.h:snap.height};
 for(const auto&j:jobs){if(!j.selected||j.match!=commandMatch())continue;int x=0,y=0;external::project(projection,j.x,j.y,j.z,x,y);
  for(auto point:j.points){int tx=0,ty=0;external::project(projection,int(point.cell%1000)*256+128,int(point.cell/1000)*256+128,point.z,tx,ty);int a=x,b=y,c=tx,d=ty;if(nativeviewhost::localPoint(transform,a,b)&&nativeviewhost::localPoint(transform,c,d))out.push_back({a,b,c,d});x=tx;y=ty;if(out.size()>=256)return out;}
 }return out;}
}

