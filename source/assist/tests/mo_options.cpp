static void optionField(moorders::Options&o,int n,uint32_t value){switch(n){case 0:o.range=value;break;case 1:o.squad=value;break;case 2:o.patrolSize=value;break;case 3:o.nodeStep=value;break;case 4:o.loopClose=value;break;case 5:o.loopStep=value;break;case 6:o.commit=value;break;}}
static uint32_t optionValue(const moorders::Options&o,int n){const uint32_t values[]={o.range,o.squad,o.patrolSize,o.nodeStep,o.loopClose,o.loopStep,o.commit};return values[n];}
int main(){
 check(sizeof(moorders::Options)==28&&moorders::validOptions({}));
 // Default geometry remains bit-for-bit equal to the pre-options production
 // helper over 320 deterministic open/closed/short/long input strokes.
 uint32_t random=1234567;auto next=[&](){random=random*1664525u+1013904223u;return int((random>>8)%9)-4;};
 for(int test=0;test<320;++test){
  moorders::Point nodes[64]{},now[16]{};historical::assist::moorders::Point oldNodes[64]{},old[16]{};int n=1+test%64,x=100,y=100;
  for(int i=0;i<n;++i){x+=next();y+=next();nodes[i]={x,y,0};oldNodes[i]={x,y,0};}if(test%2&&n>3){nodes[n-1]=nodes[0];oldNodes[n-1]=oldNodes[0];}
  int a=moorders::patrol(nodes,n,now),b=historical::assist::moorders::patrol(oldNodes,n,old);check(a==b);for(int i=0;i<a;++i)check(now[i].x==old[i].x&&now[i].y==old[i].y&&now[i].z==old[i].z);
 }
 // Invalid wire policy is rejected; a zeroed ctypes/IPC allocation cannot
 // quietly replace a valid captured policy. Limits include unsigned garbage.
 for(int field=0;field<7;++field)for(uint32_t bad:{0u,0xFFFFFFFFu,1000u}){
  moorders::Options o;optionField(o,field,bad);if(field==6&&bad==0)check(moorders::validOptions(o));else check(!moorders::validOptions(o));
 }
 for(int width:{2,3,4,5,16})for(int dx=-9;dx<=9;++dx){
  start(1);moMode=moorders::Fuzzy;sh->moFeatures=moorders::Fuzzy;moCapturedOptions.range=uint32_t(width);moHover=0;put(readMapCell(30,30)+0xE4,0u);put(targetA+0x30,0u);put(targetA+0x21C,lastHouse+0x1000u);put(readMapCell(30+dx,30)+0xE4,targetA);
  moFindTargets();check(moTargetCount==(dx>=-width/2&&dx<width-width/2?1:0));moUpdateOutline();check(moPreview.nodes[0].x==(30-width/2)*256+128&&moPreview.nodes[1].x==(30+width-width/2)*256+128);
 }
 for(int squad:{2,4,8,16}){
  start(12);moMode=moorders::Fuzzy;sh->moFeatures=moorders::Fuzzy;moCapturedOptions.squad=squad;actionValue=5;put(targetA+0x21C,lastHouse+0x1000u);put(targetB+0x21C,lastHouse+0x1000u);allocate();check(moQueueCount==12);int groups=std::clamp(12/squad,1,2);check(count(targetA)==(groups==1?12:6)&&count(targetB)==(groups==1?0:6));
  start();moCapturedOptions.squad=squad;actionValue=5;put(targetA+0x21C,lastHouse+0x1000u);put(targetB+0x21C,lastHouse+0x1000u);allocate();int first=std::min(squad,10),second=std::min(squad,10-first);check(count(targetA)==10-second&&count(targetB)==second);
 }
 for(int size:{2,4,5,16}){moorders::Options o;o.patrolSize=size;moorders::Point point{30,30},corners[16]{};check(moorders::patrol(&point,1,corners,o)==4);check(corners[0].x==30-size/2&&corners[2].x==30+size/2&&corners[0].y==30-size/2&&corners[2].y==30+size/2);}
 start(1);moMode=moorders::Patrol;sh->moFeatures=moorders::Patrol;moCapturedOptions.nodeStep=4;moStrokeCount=0;moorders::sample(moStroke,moStrokeCount,{30,30});moSamplePreview({31,30});moSamplePreview({33,30});check(moStrokeCount==1&&moCurrent.x==33);moSamplePreview({34,30});check(moStrokeCount==2&&moStroke[1].x==34);
 moorders::Point curve[]={{20,20},{30,20},{30,30},{24,20}},out[16]{};moorders::Options o;check(moorders::patrol(curve,4,out,o)==4);o.loopClose=5;check(moorders::patrol(curve,4,out,o)>4);
 moorders::Point square[]={{20,20},{30,20},{30,30},{20,30},{20,20}};o={};o.loopStep=2;check(moorders::patrol(square,5,out,o)==16);o.loopStep=16;check(moorders::patrol(square,5,out,o)==4);
 // Parameter changes invalidate the same capture as a changed gesture, for
 // both held previews and already queued-but-unsent commands.
 for(int field=0;field<7;++field){
  start(1);held[VK_SHIFT]=true;moHolding=true;tick();check(moHolding);auto options=sh->moOptions;optionField(options,field,field==6?0:optionValue(options,field)+1);sh->moOptions=options;tick();check(!moHolding&&!moOrdersPending()&&events.empty());
  start(1);allocate();sh->moOptions=options;tick();check(events.empty()&&!moOrdersPending());
 }
 start(1);check(moReadGestures());auto saved=moRuntimeOptions;std::memset(&sh->moOptions,0,sizeof sh->moOptions);check(!moReadGestures()&&moorders::sameOptions(moRuntimeOptions,saved));
 start(1);sh->moGesturePolicySeq=1;check(!moReadGestures());sh->moGesturePolicySeq=2;check(moReadGestures());
 start(1);check(moReadGestures());saved=moRuntimeOptions;sh->moOptions.range=8;barrierHook=[](){sh->moGesturePolicySeq+=2;};check(!moReadGestures()&&moorders::sameOptions(moRuntimeOptions,saved));
 // Auto-Commit=0 leaves the verified native graph pending. A deferred REAL
 // native planning key-up is still returned; no synthetic Commit is invented.
 for(int commit:{0,1})for(bool nativeRelease:{false,true}){
  start(1);sh->moFeatures=moorders::Patrol;moMode=moorders::Patrol;sh->moOptions.commit=commit;moCapturedOptions=sh->moOptions;moRouteCount=4;moorders::Point point{30,30};moorders::patrol(&point,1,moRoute,moCapturedOptions);moStage=2;moStageFrame=snap.frame;MSG msg;check(moPlanningCapture(&msg));
  if(nativeRelease){moPlanningUI.native=true;moPlanningUI.primary['Z']=moPlanningUI.watched['Z']=true;moPlanningUI.releases[0]=MSG{};moPlanningUI.releases[0].message=WM_KEYUP;moPlanningUI.releases[0].wParam='Z';moPlanningUI.count=1;}
  graph(false);tick();check(moStage==4&&events.size()==1&&events[0].eventKind==vanilla::EventType::PlanConnect);graph(true);tick();
  check(!moStage&&!moWaypointSubmitted&&sh->moOrderStatus==2);check(nativeRelease?posted==1:posted==0);check(events.size()==size_t(commit&&!nativeRelease?2:1));if(commit&&!nativeRelease)check(events.back().eventKind==vanilla::EventType::PlanCommit);
 }
 std::printf("{\"passed\":true,\"checks\":%d,\"default_geometry_cases\":320,\"scope\":\"production Options/geometry/capture/patrol with modeled native IO\"}\n",checks);
}
