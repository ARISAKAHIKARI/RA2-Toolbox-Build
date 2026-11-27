// Host publishes a small coordinate request, not a world graph. A reply is
// visual feedback only; the physical click and game thread revalidate it.
static bool offlineToolPoint(offlinetools::Point&out,POINT clientPoint,int clientW,int clientH,uint32_t generation,offlinetools::Mode mode){
 if(!shared||!offlinetools::offline(snap.session)||!mode||!(shared->offlineToolMask&uint32_t(mode))||clientW<=0||clientH<=0)return false;
 auto read=[](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);};
 uint32_t tactical=0;
 if(offlinetools::current(read)!=mode||!offlinetools::denied(mode,read)||
    !read(0xA83D4C,&out.house,4)||out.house<0x10000||!read(0xA8ED84,&out.frame,4)||out.frame<0||
    !read(0x887324,&tactical,4)||tactical<0x10000||!read(tactical+0xB0,&out.cameraX,4)||!read(tactical+0xB4,&out.cameraY,4)||
    !read(0x8A00A4,&out.width,4)||!read(0x8A00A8,&out.height,4)||out.width<=0||out.height<=0||out.width>32768||out.height>32768)return false;
 if(clientPoint.x<0||clientPoint.y<0||clientPoint.x>=clientW||clientPoint.y>=clientH)return false;
 out.x=int(int64_t(clientPoint.x)*out.width/clientW);out.y=int(int64_t(clientPoint.y)*out.height/clientH);
 out.match=commandMatch();out.generation=generation;out.tool=mode;out.tick=GetTickCount();return out.match!=0;
}
static bool offlineToolHint(const offlinetools::Point&p,offlinetools::Hint&h){
 if(!shared)return false;const auto seq=shared->offlineToolHintSeq;if(seq&1)return false;
 MemoryBarrier();h=shared->offlineToolHint;MemoryBarrier();
 return seq==shared->offlineToolHintSeq&&offlinetools::usable(h,p,GetTickCount());
}
static bool serviceOfflineToolCursor(){
 if(!shared||!shared->offlineToolMask||!offlinetools::offline(snap.session))return false;
 auto read=[](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);};
 const auto mode=offlinetools::current(read);if(!mode)return false;
 commandcursor::clear();
 if(!(shared->offlineToolMask&uint32_t(mode)))return true;
 POINT screen{},client{};RECT r{};nativeviewhost::Transform transform{};DWORD targetPid=0;
 if(!GetCursorPos(&screen)||!GetClientRect(game,&r)||!nativeviewhost::sampleTransform(shared,transform))return true;
 GetWindowThreadProcessId(WindowFromPoint(screen),&targetPid);if(targetPid!=pid)return true;
 client=screen;if(!ScreenToClient(game,&client))return true;
 offlinetools::Point p{};if(!offlineToolPoint(p,client,r.right,r.bottom,transform.generation,mode))return true;
 static offlinetools::Point last{};static DWORD submitted=0;
 const auto now=GetTickCount();
 if(!offlinetools::samePoint(last,p)||DWORD(now-submitted)>=32){
  if(!submitted||DWORD(now-submitted)>=16){last=p;submitted=now;
   InterlockedIncrement(&shared->offlineToolPointSeq);shared->offlineToolPoint=p;MemoryBarrier();InterlockedIncrement(&shared->offlineToolPointSeq);}
 }
 offlinetools::Hint h{};
 if(offlineToolHint(p,h))commandcursor::publish({pid,p.match,now,0,mode==offlinetools::Repair?commandcursor::OfflineRepair:commandcursor::OfflineSell,screen.x,screen.y});
 return true;
}
// Consume only a real left down/up pair matching the same validated hover
// target and camera. Native input is never swallowed by this feature.
static bool captureOfflineToolClick(const gameinput::PointerPress&e,const nativeviewhost::Transform&viewport){
 static offlinetools::Point down{};static offlinetools::Hint target{};static uint32_t serial=0,revision=0;static bool armed=false;
 if(e.operation!=6&&e.operation!=7){armed=false;return false;}
 auto read=[](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);};
 const auto mode=offlinetools::current(read);
 if(!mode){armed=false;return false;}
 if(!shared||!(shared->offlineToolMask&uint32_t(mode))){armed=false;return true;}
 offlinetools::Point p{};offlinetools::Hint h{};
 const bool valid=!e.keys[16]&&!e.keys[17]&&!e.keys[18]&&offlineToolPoint(p,e.point,e.width,e.height,viewport.generation,mode)&&offlineToolHint(p,h);
 if(e.operation==7){armed=valid;down=p;target=h;serial=e.serial;revision=settingsRevision;return true;}
 const bool pair=armed&&valid&&serial==e.serial&&revision==settingsRevision&&offlinetools::samePoint(down,p)&&
  DWORD(p.tick-down.tick)<=1000&&target.pointer==h.pointer&&target.id==h.id;
 armed=false;if(!pair)return true;
 auto&box=shared->pointer;if(box.request!=box.ack)return true;
 pointerinput::Intent i{};i.tick=e.tick;i.inputSerial=e.serial;i.match=p.match;i.house=p.house;i.frame=p.frame;
 i.operation=mode==offlinetools::Repair?pointerinput::OfflineRepair:pointerinput::OfflineSell;i.bindings=pointerinput::LeftClick;
 i.x=p.x;i.y=p.y;i.width=p.width;i.height=p.height;i.cameraX=p.cameraX;i.cameraY=p.cameraY;i.count=1;i.sources[0]={h.id,h.pointer};
 shared->pointerViewportGeneration=viewport.generation;box.intent=i;MemoryBarrier();InterlockedIncrement((volatile LONG*)&box.request);wakeGame();return true;
}
