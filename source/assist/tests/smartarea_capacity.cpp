// Appended to a source-extracted production fixture by smartarea_capacity.py.
// Native engine invocation/Win32 IO are controlled boundaries, never a game.
static constexpr uint32_t targetA=0x170000,targetB=0x180000;
static uint32_t actorAt(int n){return 0x10000+uint32_t(n)*0x2000;}
static void actor(uint32_t p,uint32_t id,int k=1,double size=1){
 put(p,k==1?0x7EB058u:k==2?0x7F5C70u:0x7E22A4u);put(p+0x10,id);put(p+0x21C,lastHouse);put(p+0x6C,100);put<uint8_t>(p+0x74,1);put<uint8_t>(p+0x90,1);
 uint32_t t=0x200000+(id%64)*0x2000;put(p+(k==1?0x6C0:0x6C4),t);put(t+0x380,size);put<uint8_t>(t+0xEB4,1);put<uint8_t>(t+0xD2E,1);put(t+0xA0,100);put(p+0x9C,15*256+128);put(p+0xA0,20*256+128);
}
static void used(uint32_t p,int n){
 put(p+0x114,n);uint32_t base=p==targetA?0x80000u:0xA0000u;put(p+0x118,n?base:0u);
 for(int i=0;i<n;++i){uint32_t q=base+i*0x1000;put(q,0x7EB058u);put(q+0x6C0,0x2D0000u);put(0x2D0000+0x380,1.0);put(q+0x30,i+1<n?q+0x1000:0u);}
}
static void building(uint32_t p,uint32_t id,int cap,int present=0,int flag=0x16AF){
 uint32_t t=p==targetA?0x1A0000u:0x1B0000u;put(p,0x7E3EBCu);put(p+0x10,id);put(p+0x21C,lastHouse);put(p+0x520,t);put(p+0x6C,100);put<uint8_t>(p+0x74,1);put<uint8_t>(p+0x90,1);put(t+0xA0,100);put(t+0x634,1);put<uint8_t>(t+flag,1);put(t+0x5E0,cap);put(t+0x388,128.0);put(p+0x9C,(p==targetA?30:45)*256+128);put(p+0xA0,30*256+128);used(p,present);
}
static void start(int n=10,int a=10,int b=10,int au=0,int bu=0){
 moPlanningUI={};setup();resetMoOrders();posted=0;actors.clear();selected.clear();entryReservationCount=0;actorLeaseArbiter.clear();actorFrameArbiter.frame=-1;missionObserver.clear();events.clear();commands.clear();beforeQueue={};highLevelCalls=constructCalls=0;highBackend=false;actionValue=3;actionOK=true;failAt=0;
 for(int i=0;i<n;++i){uint32_t p=actorAt(i);actor(p,100+i);actors.push_back(p);selected.push_back(p);}
 building(targetA,900,a,au);building(targetB,901,b,bu);actors.push_back(targetA);actors.push_back(targetB);put(targetA+0x30,targetB);put(readMapCell(30,30)+0xE4,targetA);
 sh->moFeatures=moorders::Area;moEpoch=match;moHouse=lastHouse;moMode=moorders::Area;moHover=targetA;moCurrent={30,30};moStrokeCount=0;moHolding=false;moCapturedGestures=sh->moGestures;moStarted=clockValue;moStartFrame=snap.frame;moStage=0;
#ifndef EXPECT_BASELINE
 moCapturedOptions=sh->moOptions;
#endif
 check(moCapture());
}
static void allocate(){moAssignments(false);}
static int count(uint32_t target){int n=0;for(int i=0;i<moQueueCount;++i)if(moQueue[i].targetPointer==target)++n;return n;}
static void tick(){++snap.frame;put(0xA8ED84,snap.frame);clockValue+=30;sh->heartbeat=clockValue;moOrdersTick(lastHouse,snap.frame,clockValue);}
int main(){
#ifdef EXPECT_BASELINE
 start(1);actionValue=1;check(moActionMission(actorAt(0),targetA)==-1);
 start();allocate();check(count(targetA)==6&&count(targetB)==4);
 start(1,1,0);allocate();highBackend=true;snap.session=3;put(0xA8B238,3);beforeQueue=[](){used(targetA,1);};tick();check(highLevelCalls==1&&events.size()==1);
 std::printf("{\"passed\":true,\"checks\":%d,\"counterexamples\":[\"SelectMove-no-Enter\",\"ten-infantry-six-four\",\"high-backend-bypasses-post-clock-capacity\"]}\n",checks);return 0;
#else
 // Select/Move is corrected only for verified own compatible facilities.
 for(unsigned action:{1u,2u})for(int sk:{1,2})for(int flag:{0x16AF,0x16AE,0x16AB}){
  start(1);actionValue=action;uint32_t p=actorAt(0);actor(p,100,sk);building(targetA,900,4,0,flag);
  // start already set InfantryAbsorb: clear it when testing a different flag.
  uint32_t t=typeOf(targetA,3);put<uint8_t>(t+0x16AF,flag==0x16AF);
  bool compatible=(sk==1&&flag==0x16AF)||(sk==2&&flag!=0x16AF);
  check(moActionMission(p,targetA)==(compatible?7:-1));
 }
 start(1);actionValue=1;put<uint8_t>(typeOf(actorAt(0),1)+0xEC3,1);check(moActionMission(actorAt(0),targetA)==-1);
 for(uint32_t owner:{lastHouse+0x1000u,0u}){start(1);actionValue=1;put(targetA+0x21C,owner);check(moActionMission(actorAt(0),targetA)==-1);}
 start(1);actionValue=1;put<uint8_t>(actorAt(0)+0x134,1);check(moActionMission(actorAt(0),targetA)==-1);
 start(1);actionValue=1;put<uint8_t>(actorAt(0)+0x1C8,1);check(moActionMission(actorAt(0),targetA)==-1);
 start(1);actionValue=1;put(actorAt(0)+0x11C,targetB);check(moActionMission(actorAt(0),targetA)==-1);
 start(1);actionValue=1;actionOK=false;check(moActionMission(actorAt(0),targetA)==-1);
 start(1);actionValue=1;failAt=typeOf(targetA,3)+0x16AF;check(moActionMission(actorAt(0),targetA)==-1);
 // True remaining slots, not distance or four-per-target, govern distribution.
 start();allocate();check(moQueueCount==10&&count(targetA)==5&&count(targetB)==5);
 for(int n=1;n<=12;++n)for(int a=0;a<=6;++a)for(int b=0;b<=6;++b){
  start(n,a+2,b+2,2,2);allocate();int expectedA=std::min(a,std::max((n+1)/2,n-b));int expectedB=std::min(b,n-expectedA);
  check(count(targetA)==expectedA&&count(targetB)==expectedB&&moQueueCount==std::min(n,a+b));
  for(int i=0;i<moQueueCount;++i)check(moQueue[i].mission==7&&moQueue[i].entrySlots==1);
 }
 start(6,6,6);for(int i=0;i<6;++i)put(typeOf(actorAt(i),1)+0x380,2.0);allocate();check(count(targetA)==3&&count(targetB)==3);
 start(8,2,3);allocate();check(moQueueCount==5&&count(targetA)==2&&count(targetB)==3);
 start(6,4,4);failAt=targetA+0x114;allocate();check(count(targetA)==0&&count(targetB)==4);
 // A native reservation and its pending record refer to one actor, not two.
 start(4,4,0);uint32_t prior=0x70000;actor(prior,50,1,2);actors.push_back(prior);put(prior+0xAC,7);put(prior+0x500,targetA);entryReservations[entryReservationCount++]={50,900,typeOf(targetA,3),0,100,2};allocate();check(moQueueCount==2&&count(targetA)==2);
 tick();check(events.size()==2&&entryReservationCount==3);selected={actorAt(2),actorAt(3)};check(moCapture());allocate();check(moQueueCount==0); // already queued entrants still consume room
 // Capacity changed before dispatch: discard the stale tail, never overbook.
 start(2,2,0);allocate();used(targetA,2);tick();check(events.empty()&&entryReservationCount==0&&sh->moOrderRejected==2);
 // Post-clock changes are rechecked by the real router's forced constructed
 // transport even when the user selected the optional high-level MP backend.
 for(int change=0;change<6;++change){
  start(1,1,0);allocate();highBackend=true;snap.session=3;put(0xA8B238,3);
  beforeQueue=[=](){if(change==0)used(targetA,1);else if(change==1)++manualVersions.tokens[100];else if(change==2)put(actorAt(0)+0x10,999u);else if(change==3)put(targetA+0x10,999u);else if(change==4)put(targetA+0x21C,lastHouse+0x1000u);else failAt=targetA+0x114;};
  tick();check(events.empty()&&entryReservationCount==0&&highLevelCalls==0&&constructCalls==1);failAt=0;tick();check(change==5?events.size()==1&&entryReservationCount==1:events.empty());
 }
 start(1,1,0);actionValue=1;allocate();highBackend=true;snap.session=3;put(0xA8B238,3);tick();check(events.size()==1&&highLevelCalls==0&&constructCalls==1&&entryReservationCount==1);
 check(events[0].mission==7&&events[0].b==0&&events[0].c==900&&events[0].tagC==0x34&&!events[0].appendWaypoint);tick();check(events.size()==1&&entryReservationCount==1);
 start(1,1,0);allocate();entryReservationCount=cooperative::MaxSources;for(int i=0;i<entryReservationCount;++i)entryReservations[i]={uint32_t(3000+i),901,0,0,100,1};tick();check(events.empty()&&entryReservationCount==cooperative::MaxSources);
 start(1,1,0);allocate();entryReservationCount=cooperative::MaxSources;for(int i=0;i<entryReservationCount;++i)entryReservations[i]={uint32_t(3000+i),901,0,0,100,1};entryReservations[0].id=100;tick();check(events.size()==1&&entryReservationCount==cooperative::MaxSources&&entryReservations[entryReservationCount-1].target==900);
 // Other commands keep the original backend choice/default force=false.
 start(1);actionValue=5;moMode=moorders::Fuzzy;sh->moFeatures=moorders::Fuzzy;put(targetA+0x21C,lastHouse+0x1000u);allocate();highBackend=true;snap.session=3;put(0xA8B238,3);tick();check(events.size()==1&&highLevelCalls==1&&constructCalls==0&&entryReservationCount==0);
 std::printf("{\"passed\":true,\"checks\":%d,\"capacity_matrix_cases\":588,\"scope\":\"actual bridge/GameView/reservation/nativeMission with controlled queue/clock boundaries\"}\n",checks);
#endif
}
