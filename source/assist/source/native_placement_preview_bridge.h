// An explicit toolbox modal cursor, entirely outside native cursor state.
// Only this mode's matched down/up pair is consumed. Sidebar and native tools
// keep their messages; an unrelated native cursor cancels this mode first.
static nativeplacement::Preview placementView;
static uint32_t placementPreviewSerial=0,placementCapturedButtons=0;
static uint32_t placementCapturedSerial[2]{};
static void nativePlacementPublish(){
 if(!sh)return;placementView.tick=sampleClock(sh);placementView.match=match;
 InterlockedIncrement(&sh->placementPreviewSeq);MemoryBarrier();sh->placementPreview=placementView;MemoryBarrier();InterlockedIncrement(&sh->placementPreviewSeq);notifyHost();
}
static void nativePlacementReset(){placementView={};++placementPreviewSerial;nativePlacementPublish();}
static bool nativePlacementCursorFree(){
 uint8_t tools[5]{};uint32_t state[4]{};int super=-1;
 // Map::CurrentBuilding/Type and their copy, the five native tool modes,
 // and the selected superweapon are read only. Never clear someone else's UI.
 return read(0x87F7E8+0x1190,state,8)&&!state[0]&&!state[1]&&
  read(0x87F7E8+0x11A4,state+2,8)&&!state[2]&&!state[3]&&
  read(0x87F7E8+0x11B0,tools,sizeof tools)&&!tools[0]&&!tools[1]&&!tools[2]&&!tools[3]&&!tools[4]&&
  read(0x8809A0,&super,4)&&super<0&&
  !sh->formationPreview.active&&!sh->moOrdersPreview.active&&!sh->viewZoomBusy;
}
static bool nativePlacementInputContext(bool preview=false){
 if(!nativePlacementAvailable()||DWORD(sampleClock(sh)-sh->heartbeat)>(preview?1000u:150u)||!nativePlacementCursorFree())return false;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);return foreground==GetCurrentProcessId();
}
static int nativePlacementArm(uint32_t house,DWORD now){
 if(placementView.active){nativePlacementReset();return 2;}
 if(!nativePlacementInputContext())return -4;
 int tab=-1,after=-1;nativeplacement::Offer offer{};
 if(!read(0x87F7E8+0x539C,&tab,4)||tab<0||tab>1||!nativePlacementCapture(house,tab,now,offer)||
    !read(0x87F7E8+0x539C,&after,4)||after!=tab)return -7;
 placementView={};placementView.offer=offer;placementView.active=1;++placementPreviewSerial;
 int foundation=-1;
 if(read(offer.type+0xEF0,&foundation,4)&&foundation>=0&&foundation<21){
  read(0x8192B8+uint32_t(foundation)*4,&placementView.width,4);read(0x819310+uint32_t(foundation)*4,&placementView.height,4);
  if(placementView.width<1||placementView.width>32||placementView.height<1||placementView.height>32)placementView.width=placementView.height=0;
 }
 nativePlacementPublish();return 2; // ARMED; no Place event has been constructed.
}
static bool nativePlacementRenew(DWORD now,bool preview=false){
 if(!placementView.active||!nativePlacementInputContext(preview))return false;
 int tab=-1;nativeplacement::Offer current{};
 if(!read(0x87F7E8+0x539C,&tab,4)||tab!=placementView.offer.tab||
    !nativePlacementCapture(lastHouse,tab,now,current)||!nativeplacement::sameIdentity(current,placementView.offer))return false;
 placementView.offer=current;return true;
}
static bool nativePlacementWindow(HWND window){
 if(!window||uintptr_t(window)!=get<uint32_t>(0xB73550))return false;
 DWORD owner=0;return GetWindowThreadProcessId(window,&owner)&&owner==GetCurrentProcessId();
}
static bool nativePlacementPoint(HWND window,POINT point,nativeplacement::Request&r,NativePlacementViewport&view){
 r={};r.offer=placementView.offer;RECT client{};uint32_t tactical=0;
 if(!nativePlacementWindow(window)||!GetClientRect(window,&client)||client.right<=0||client.bottom<=0||point.x<0||point.y<0||point.x>=client.right||point.y>=client.bottom||
    !read(0x887324,&tactical,4)||tactical<0x10000||!read(tactical+0xB0,&r.cameraX,4)||!read(tactical+0xB4,&r.cameraY,4)||
    !read(0x8A00A4,&r.width,4)||!read(0x8A00A8,&r.height,4))return false;
 r.screenX=int(int64_t(point.x)*r.width/client.right);r.screenY=int(int64_t(point.y)*r.height/client.bottom);
 static const uint8_t query[]={0x8B,0x44,0x24,0x10,0x83,0xEC,0x18,0x8B};
 if(!nativePlacementViewport(r,view)||!nativePlacementSignature(0x692300,query,sizeof query))return false;
 struct Point{int x,y;}p{r.screenX-view.x,r.screenY-view.y};vanilla::Cell cell{};struct Coord{int x,y,z;}coord{};uint32_t object=0;uint8_t a=0,b=0;
 call<void>(0x692300,0x87F7E8,&p,&cell,&coord,&object,&a,&b);
 NativePlacementViewport after{};if(!nativePlacementViewport(r,after)||!(after==view)||!nativeplacement::targetUsable([](uint32_t at,void*out,size_t n){return read(at,out,n);},cell.x,cell.y))return false;
 r.cellX=cell.x;r.cellY=cell.y;placementView.cellX=cell.x;placementView.cellY=cell.y;
 placementView.x=cell.x*256;placementView.y=cell.y*256;placementView.z=coord.z;
 double z=.25;if(read(0xB0CD48,&z,8)&&z>0&&z<=1)placementView.zScale=z;
 return true;
}
static bool nativePlacementInteractiveValid(const NativePlacementSubmission&s){
 return placementView.active&&s.serial==placementPreviewSerial&&nativePlacementInputContext()&&
  nativeplacement::sameIdentity(s.request.offer,placementView.offer);
}
static void nativePlacementTick(DWORD now){
 if(!placementView.active)return;
 if(!nativePlacementRenew(now,true)){nativePlacementReset();return;}
 HWND window=reinterpret_cast<HWND>(uintptr_t(get<uint32_t>(0xB73550)));POINT point{};
 nativeplacement::Request request{};NativePlacementViewport view{};
 placementView.status=GetCursorPos(&point)&&ScreenToClient(window,&point)&&nativePlacementPoint(window,point,request,view)?1:0;
 nativePlacementPublish();
}
static bool nativePlacementMessage(MSG*msg){
 const bool leftDown=msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONDBLCLK,leftUp=msg->message==WM_LBUTTONUP;
 const bool rightDown=msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONDBLCLK,rightUp=msg->message==WM_RBUTTONUP;
 const uint32_t bit=leftDown||leftUp?1u:rightDown||rightUp?2u:0u;
 const unsigned button=bit==2?1:0;
 // A new physical press proves an old release was missed outside this input
 // route. Never let that old ownership swallow the new native click's up.
 if(leftDown||rightDown){placementCapturedButtons&=~bit;placementCapturedSerial[button]=0;}
 const bool ownedUp=(leftUp||rightUp)&&(placementCapturedButtons&bit);
 auto consume=[&](){msg->message=WM_NULL;msg->wParam=msg->lParam=0;return true;};
 if(ownedUp){
  const uint32_t captured=placementCapturedSerial[button];placementCapturedSerial[button]=0;
  placementCapturedButtons&=~bit;
  if(leftUp&&captured==placementPreviewSerial&&nativePlacementWindow(msg->hwnd)&&nativePlacementRenew(sampleClock(sh))){
   NativePlacementSubmission submission{};submission.interactive=true;submission.serial=placementPreviewSerial;
   POINT point{short(LOWORD(msg->lParam)),short(HIWORD(msg->lParam))};
   if(nativePlacementPoint(msg->hwnd,point,submission.request,submission.view)){
    const auto&r=submission.request;const auto order=intent::placeBuilding(get<int>(lastHouse+0x30),r.offer.typeIndex,r.offer.naval,{short(r.cellX),short(r.cellY)});
    if(validateNativePlacement(order,&submission)&&submitConstructed(order,OrderAuthority::Local,&submission))nativePlacementReset();
   }
  }
  return consume();
 }
 if(!placementView.active)return false;
 if(!nativePlacementRenew(sampleClock(sh),true)){nativePlacementReset();return false;}
 if((msg->message==WM_KEYDOWN||msg->message==WM_SYSKEYDOWN)&&msg->wParam==VK_ESCAPE){nativePlacementReset();return consume();}
 if(!leftDown&&!rightDown)return false;
 nativeplacement::Request request{};NativePlacementViewport view{};POINT point{short(LOWORD(msg->lParam)),short(HIWORD(msg->lParam))};
 if(!nativePlacementPoint(msg->hwnd,point,request,view)){nativePlacementReset();return false;} // Sidebar click remains native.
 placementCapturedButtons|=bit;placementCapturedSerial[button]=placementPreviewSerial;if(rightDown)nativePlacementReset();return consume();
}
