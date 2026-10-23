// Input and vanilla event submission run on the existing Runtime boundary.
// Voxel decoding/rasterization lives exclusively in the external controller.
static formation::Preview formationView;
static formation::Policy formationPolicy;
static bool formationModal=false,formationHolding=false,formationButton=false;
static bool formationSuppressed[256]{};
static uint32_t formationAltAt=0,formationEpoch=0,formationLayoutTick=0;
static int formationStartX=0,formationStartY=0,formationLastX=0,formationLastY=0,formationPixelX=0,formationPixelY=0;
static bool formationDirected=false,formationHoldConsumed=false;
struct FormationJob {formation::Unit unit;formation::Slot goal;uint32_t approach=0,epoch=0,issued=0;int stage=0,frame=0;bool observed=false;};
static FormationJob formationJobs[formation::Maximum];static int formationJobCursor;
static bool formationOwns(uint32_t id){for(int n=0;n<formationJobCount;++n)if(formationJobs[n].unit.id==id&&formationJobs[n].unit.token==manualVersions.get(id))return true;return false;}
static void formationPublish(){if(!sh)return;formationView.tick=sampleClock(sh);formationView.match=match;++formationView.revision;
 InterlockedIncrement(&sh->formationSeq);MemoryBarrier();sh->formationPreview=formationView;MemoryBarrier();InterlockedIncrement(&sh->formationSeq);sh->formationJobs=formationJobCount;notifyHost();}
static void formationCancel(){formationView.active=formationView.dragging=0;formationView.count=0;formationModal=formationHolding=false;formationPublish();}
static void resetFormation(){formationView={};formationModal=formationHolding=formationButton=false;formationAltAt=0;formationHoldConsumed=false;formationJobCount=formationJobCursor=0;memset(formationSuppressed,0,sizeof formationSuppressed);if(sh){sh->formationJobs=0;formationPublish();}}
static bool formationContext(){
 if(!sh||fault||sh->stop||!sh->foreground||!gameentry::active(gameentry::Client::Assist)||snap.status!=1||!match||!lastHouse||snap.commandStatus!=1||!get<bool>(0xA8E9A0)||get<uint32_t>(0xA83D4C)!=lastHouse||get<bool>(0xA8BC60+25)||DWORD(sampleClock(sh)-sh->heartbeat)>150)return false;
 int frame=get<int>(0xA8ED84);if(frame<snap.frame||int64_t(frame)-snap.frame>6||frame<0||frame>=INT32_MAX||get<int>(0xA8B238)!=snap.session||!eventdelivery::mayQueue(snap.session)||get<bool>(lastHouse+0x1F5)||get<bool>(lastHouse+0x1F6)||get<bool>(lastHouse+0x1F8))return false;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return false;
 if(!external::display::inspect([](uint32_t at,void*out,size_t n){return read(at,out,n);}).visible())return false;
 long seq=sh->formationPolicySeq;if(seq&1)return formationPolicy.enabled!=0;MemoryBarrier();auto p=sh->formationPolicy;MemoryBarrier();if(seq!=sh->formationPolicySeq)return formationPolicy.enabled!=0;formationPolicy=p;return p.enabled!=0;
}
static bool formationActor(uint32_t p){int air=0,k=kind(p);return p&&worldObjects.contains(p)&&own(p,lastHouse)&&(k==1||k==2)&&!get<bool>(p+0x1C8)&&!get<uint32_t>(p+0x11C)&&!get<uint32_t>(p+0x2B0)&&!get<bool>(p+0x427)&&GuardedObjectAir(p,&air)&&!air;}
static void formationSelect(){
 indexWorld(get<int>(0xA8ED84));formationView.count=0;Vec selected=vec(0xA8ECB8);
 for(int n=0;n<selected.count&&formationView.count<formation::Maximum;++n){uint32_t p=item(selected,n);if(!formationActor(p))continue;
  auto&u=formationView.units[formationView.count++];u={};u.pointer=p;u.id=get<uint32_t>(p+0x10);u.type=typeOf(p,kind(p));u.token=manualVersions.get(u.id);u.x=get<int>(p+0x9C);u.y=get<int>(p+0xA0);u.z=get<int>(p+0xA4);u.kind=kind(p);
 }
}
static bool formationPoint(MSG*msg,int&x,int&y,int&px,int&py){
 RECT r{};if(!GetClientRect(msg->hwnd,&r)||r.right<=0||r.bottom<=0)return false;uint32_t tactical=get<uint32_t>(0x887324);if(!tactical)return false;
 pointerinput::Intent point{};point.width=get<int>(0xA8EB84);point.height=get<int>(0xA8EB88);point.cameraX=get<int>(tactical+0xB0);point.cameraY=get<int>(tactical+0xB4);point.operation=pointerinput::AirWaypoint;
 px=point.x=int(int64_t(short(LOWORD(msg->lParam)))*point.width/r.right);py=point.y=int(int64_t(short(HIWORD(msg->lParam)))*point.height/r.bottom);
 uint32_t c=pointerCell,t=pointerTarget,id=pointerTargetID;int z=pointerGroundZ;bool ok=pointerLocation(point);if(ok){x=int(pointerCell%1000)*256+128;y=int(pointerCell/1000)*256+128;}
 pointerCell=c;pointerTarget=t;pointerTargetID=id;pointerGroundZ=z;return ok;
}
static bool formationCell(int x,int y){
 if(x<=0||y<=0||x>=512||y>=512)return false;uint32_t cell=readMapCell(x,y);if(!cell)return false;
 // Preview omits permanent obstacles, but native movement owns terrain,
 // locomotor, congestion and reachability decisions. No pathfinder or RNG probe.
 uint32_t q=get<uint32_t>(cell+0xE4);for(int n=0;q>=0x10000&&n<128;++n){if(get<uint32_t>(q)==0x7F522C&&!get<bool>(q+0xCD))return false;if(worldObjects.contains(q)&&kind(q)==3&&live(q))return false;uint32_t next=get<uint32_t>(q+0x30);if(q==next)return false;q=next;}
 return q<0x10000;
}
static void formationLayout(int x,int y,int px,int py){
 formationLayoutTick=sampleClock(sh);
 const int count=formationView.count;if(count<=0)return;
 double pixels=std::hypot(double(px-formationStartX),double(py-formationStartY));bool click=formationView.dragging&&pixels<6;
 formationDirected=!click;
 if(!click&&formationView.dragging)formationView.facing=formation::dragFacing(double(px)-formationStartX,double(py)-formationStartY,formationView.facing);
 double stretch=!formationView.dragging||click?0:std::clamp((pixels-6)/314.,0.,1.);formation::Point points[formation::Maximum];formation::layout(count,formationView.shape,stretch,click,points);
 double fx=std::cos(formationView.facing),fy=std::sin(formationView.facing);
 for(int n=0;n<count;++n){int cx=int(std::lround((formationView.ax-128)/256.+points[n].x*(-fy)-points[n].y*fx));int cy=int(std::lround((formationView.ay-128)/256.+points[n].x*fx-points[n].y*fy));
  formation::Slot result{};
  // Quantization of rotated ranks must not put two tanks in the same cell.
  for(int radius=0;radius<=4&&!result.valid;++radius)for(int oy=-radius;oy<=radius&&!result.valid;++oy)for(int ox=-radius;ox<=radius&&!result.valid;++ox){if(std::max(std::abs(ox),std::abs(oy))!=radius)continue;int tx=cx+ox,ty=cy+oy;if(!formationCell(tx,ty))continue;bool used=false;
   for(int j=0;j<n;++j)if(formationView.slots[j].valid&&formationView.slots[j].x==tx*256+128&&formationView.slots[j].y==ty*256+128){used=true;break;}if(used)continue;
   struct Coord{int x,y,z;}c{tx*256+128,ty*256+128,0};result={c.x,c.y,call<int>(0x578080,0x87F7E8,&c),1};
  }formationView.slots[n]=result;
 }
 formation::assign(formationView.units,count,formationView.slots);formationLastX=x;formationLastY=y;formationPixelX=px;formationPixelY=py;formationPublish();
}
static void formationAtCursor(MSG*msg){POINT pt{};if(!GetCursorPos(&pt)||!ScreenToClient(msg->hwnd,&pt))return;MSG pointer=*msg;pointer.lParam=MAKELPARAM(short(pt.x),short(pt.y));int x,y,px,py;if(!formationPoint(&pointer,x,y,px,py))return;formationView.ax=x;formationView.ay=y;formationStartX=px;formationStartY=py;formationLayout(x,y,px,py);}
static void formationCommit(){
 sh->formationAccepted=sh->formationRejected=0;
 for(int n=0;n<formationView.count;++n){auto u=formationView.units[n];auto goal=formationView.slots[n];
  if(!goal.valid||!formationActor(u.pointer)||get<uint32_t>(u.pointer+0x10)!=u.id||manualVersions.get(u.id)!=u.token){++sh->formationRejected;continue;}
  for(int j=0;j<formationJobCount;)if(formationJobs[j].unit.id==u.id)formationJobs[j]=formationJobs[--formationJobCount];else ++j;
  if(formationJobCount>=formation::Maximum){++sh->formationRejected;continue;}
  // This is an explicit player order: invalidate older automation leases.
  cancelOreJob(u.pointer);u.token=manualVersions.get(u.id);
  int dx=int(std::lround(std::cos(formationView.facing))),dy=int(std::lround(std::sin(formationView.facing)));
  uint32_t approach=0;int cx=goal.x/256-dx,cy=goal.y/256-dy;
  if(formationDirected&&formationCell(cx,cy))approach=cy*1000+cx;
  auto&j=formationJobs[formationJobCount++];j={};j.unit=u;j.goal=goal;j.approach=approach;j.epoch=match;j.frame=get<int>(0xA8ED84);++sh->formationAccepted;
 }
 formationView.active=formationView.dragging=0;formationHoldConsumed=formationHolding;formationModal=formationHolding=false;formationAltAt=0;sh->formationConsumedUntil=sampleClock(sh)+200;formationPublish();
}
static bool formationSwitch(uint32_t key){bool found=false;for(auto k:formationPolicy.keys)found|=k&&k==key;if(!found)return false;formationView.shape=formation::cycle(formationView.shape,key,formationPolicy.keys);if(formationView.count)formationLayout(formationLastX,formationLastY,formationPixelX,formationPixelY);return true;}
static bool formationMessage(MSG*msg){
 const bool down=msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN,up=msg->message==WM_KEYUP||msg->message==WM_SYSKEYUP;
 uint32_t key=uint32_t(msg->wParam)&255;auto swallow=[&](){msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;};
 if(up&&formationSuppressed[key]){formationSuppressed[key]=false;if(key==(formationPolicy.hold&255))formationHoldConsumed=false;if(key==(formationPolicy.hold&255)){formationHolding=false;if(!formationModal&&!formationButton)formationCancel();}return swallow();}
 if(msg->message==WM_RBUTTONUP&&formationButton){formationButton=false;if(formationContext()&&formationView.active){int x,y,px,py;if(formationPoint(msg,x,y,px,py)){formationLayout(x,y,px,py);formationCommit();}else formationCancel();}else formationCancel();return swallow();}
 if(!formationContext()){if(formationView.active)formationCancel();return false;}
 if(down){bool repeated=(msg->lParam&(1u<<30))!=0;
  uint32_t mods=((GetKeyState(VK_SHIFT)&0x8000)?1:0)|((GetKeyState(VK_CONTROL)&0x8000)?2:0)|((GetKeyState(VK_MENU)&0x8000)?4:0);
  uint32_t chord=key|(mods<<8);if(key==VK_MENU)chord&=~(4u<<8);if(key==VK_SHIFT)chord&=~(1u<<8);if(key==VK_CONTROL)chord&=~(2u<<8);
  if(key==VK_MENU&&!repeated){uint32_t now=sampleClock(sh);if(formationAltAt&&DWORD(now-formationAltAt)<=350){formationModal=true;formationView.active=1;formationSelect();formationAtCursor(msg);formationAltAt=0;formationSuppressed[key]=true;formationPublish();return swallow();}else formationAltAt=now;}
  if(formationPolicy.hold&&chord==(formationPolicy.hold&0x7ff)&&runtimeKeys.qualifies(uint16_t(formationPolicy.hold))){if(formationHoldConsumed)return swallow();formationHolding=true;formationView.active=1;formationSuppressed[key]=true;if(!repeated&&!formationButton){formationSelect();formationAtCursor(msg);}formationPublish();return swallow();}
  if(formationView.active&&key==VK_ESCAPE){formationCancel();formationSuppressed[key]=true;return swallow();}
  // Ignore the mode's hold modifier while interpreting a shape binding.
  if(formationView.active){uint32_t shapeChord=chord;uint32_t hold=formationPolicy.hold&255;if(formationHolding){if(hold==VK_MENU)shapeChord&=~(4u<<8);if(hold==VK_CONTROL)shapeChord&=~(2u<<8);if(hold==VK_SHIFT)shapeChord&=~(1u<<8);}
   bool matches=false;for(auto k:formationPolicy.keys)matches|=k&&(k&0x7ff)==shapeChord;if(matches){if(!repeated){bool single=false,twice=false;for(auto k:formationPolicy.keys)if((k&0x7ff)==shapeChord){single|=!(k&0x8000);twice|=(k&0x8000)!=0;}if(twice&&(runtimeKeys.held[key]&2))formationSwitch(shapeChord|0x8000);else if(single&&!twice)formationSwitch(shapeChord);}formationSuppressed[key]=true;formationPublish();return swallow();}
  }
 }
 if(!formationView.active)return false;
 if(msg->message==WM_RBUTTONDOWN){int x,y,px,py;if(!formationPoint(msg,x,y,px,py))return false;formationSelect();if(!formationView.count)return false;formationButton=true;formationView.dragging=1;formationView.ax=x;formationView.ay=y;formationStartX=px;formationStartY=py;formationLayout(x,y,px,py);sh->formationConsumedUntil=sampleClock(sh)+200;return swallow();}
 if(msg->message==WM_MOUSEMOVE){if(DWORD(sampleClock(sh)-formationLayoutTick)<16)return formationButton?swallow():false;int x,y,px,py;if(formationPoint(msg,x,y,px,py)){if(!formationButton){formationSelect();formationView.ax=x;formationView.ay=y;formationStartX=px;formationStartY=py;}formationLayout(x,y,px,py);}if(formationButton)return swallow();}
 return false;
}
static void formationTick(uint32_t house,int frame,DWORD now){
 if(formationEpoch!=match){resetFormation();formationEpoch=match;}
 if(!formationContext()){if(formationView.active)formationCancel();if(!sh->formationPolicy.enabled)formationJobCount=0;}
 if(sh->formationKey!=sh->formationKeyAck){if(formationView.active)formationSwitch(sh->formationKeyCode);sh->formationKeyAck=sh->formationKey;formationPublish();}
 if(!simulationReady()||snap.chat)return;
 int visited=0;while(formationJobCount&&visited++<32){if(formationJobCursor>=formationJobCount)formationJobCursor=0;int at=formationJobCursor++;auto&j=formationJobs[at];uint32_t p=worldByID(j.unit.id);
  bool remove=j.epoch!=match||p!=j.unit.pointer||!formationActor(p)||manualVersions.get(j.unit.id)!=j.unit.token||frame<j.frame||frame-j.frame>1800;
  uint32_t cell=j.stage<2&&j.approach?j.approach:uint32_t(j.goal.y/256*1000+j.goal.x/256);int gx=int(cell%1000)*256+128,gy=int(cell/1000)*256+128;
  if(!remove){double dx=double(get<int>(p+0x9C))-gx,dy=double(get<int>(p+0xA0))-gy;bool arrived=dx*dx+dy*dy<=96.*96.;
   if(j.stage==1||j.stage==3){bool going=get<int>(p+0xAC)==2&&get<uint32_t>(p+0x5A4)==readMapCell(cell%1000,cell/1000);j.observed|=going;
    if(arrived){if(j.stage==1&&j.approach){j.stage=2;j.observed=false;}else remove=true;}
    else if(DWORD(now-j.issued)>15000)remove=true;
   }
   if(!remove&&(j.stage==0||j.stage==2)&&outgoingRoom()>0){uint32_t destination=j.stage==2?uint32_t(j.goal.y/256*1000+j.goal.x/256):cell;
    if(nativeMission(house,j.unit.id,2,0,destination,11,0,0,taskarbiter::Owner::Formation)){j.stage=j.stage==2?3:1;j.issued=now;}
   }
  }
  if(remove){formationJobs[at]=formationJobs[--formationJobCount];formationJobCursor=at;}
 }sh->formationJobs=formationJobCount;
}
