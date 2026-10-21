#include "native_cameos.h"
// Same manager message boundary as enhanced ground clicks. No inline hooks,
// EVA interception, simulation calls, or OutList edits are used here.
struct ReservationCapture {bool down=false;construction::InputPolicy policy{};int index=-1,x=0,y=0,width=0,height=0,top=0;uint32_t tick=0,button=0,all=0;};
static ReservationCapture reservationCapture;
static void swallowReservation(MSG*msg){msg->message=WM_NULL;msg->wParam=msg->lParam=0;}
static bool reservationContext(construction::InputPolicy&policy,DWORD now){
 if(!sh||fault||sh->stop||!gameentry::active(gameentry::Client::Assist)||!sh->foreground||
    snap.status!=1||snap.commandStatus!=1||!match||!lastHouse||!(sh->flags&ReserveConstruction)||DWORD(now-sh->heartbeat)>150)return false;
 // UI input can arrive just before the next scheduled sample. Validate live
 // context without requiring snap.frame == current frame; no event is sent.
 int frame=get<int>(0xA8ED84);uint32_t house=get<uint32_t>(0xA83D4C);
 if(house!=lastHouse||house==get<uint32_t>(0xAC1198)||get<bool>(house+0x1F5)||get<bool>(house+0x1F6)||get<bool>(house+0x1F8)||
    !get<bool>(0xA8E9A0)||frame<snap.frame||int64_t(frame)-snap.frame>6||frame<0||frame>=INT32_MAX||
    get<int>(0xA8B238)!=snap.session||!eventdelivery::mayQueue(snap.session)||get<bool>(0xA8BC60+25)||
    !external::display::inspect([](uint32_t at,void*out,size_t n){return read(at,out,n);}).visible())return false;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return false;
 long seq=sh->reservePolicySeq;if(seq&1)return false;MemoryBarrier();policy=sh->reservePolicy;MemoryBarrier();
 if(seq!=sh->reservePolicySeq||policy.match!=match||policy.house!=lastHouse||policy.tab<0||policy.tab>1||policy.slots>5||policy.count<0||policy.count>5||(!policy.slots&&!policy.count)||DWORD(now-policy.tick)>150)return false;
 uint32_t f=get<uint32_t>(lastHouse+(policy.tab?0x53CC:0x53BC));
 return f>=0x10000&&get<uint32_t>(f+0x6C)==lastHouse&&get<uint32_t>(f+0x58)==policy.parent&&
  get<uint32_t>(policy.parent+0x10)==policy.parentID&&get<int>(f+0x24)>=0&&get<int>(f+0x24)<=54;
}
static bool reservationMessage(MSG*msg){
 const bool right=msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONUP;
 const bool down=msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONDBLCLK||msg->message==WM_RBUTTONDOWN;
 const bool up=msg->message==WM_LBUTTONUP||msg->message==WM_RBUTTONUP;const uint32_t button=right?2:1;
 if(!down&&!up)return false;
 const DWORD now=sh?sampleClock(sh):0;
 // Always balance an owned DOWN, even if focus/menu/parent changed meanwhile.
 if(up&&(!reservationCapture.down||reservationCapture.button!=button))return false;
 ReservationCapture captured=reservationCapture;
 if(down&&reservationCapture.down){if(reservationCapture.button!=button)return false;swallowReservation(msg);return true;}
 if(up)reservationCapture={};
 construction::InputPolicy policy{};
 bool valid=reservationContext(policy,now)&&sh->reserveInput==sh->reserveConsumed;
 construction::NativeView frame;RECT client{};int x=0,y=0,index=-1;
 if(valid){valid=GetClientRect(msg->hwnd,&client)&&client.right>0&&client.bottom>0&&
   construction::readNativeView([](uint32_t at,void*out,size_t n){return read(at,out,n);},frame)&&frame.tab==policy.tab;
  if(valid){x=int(int64_t(short(LOWORD(msg->lParam)))*frame.width/client.right);y=int(int64_t(short(HIWORD(msg->lParam)))*frame.height/client.bottom);index=frame.hit(x,y);valid=false;
   for(int n=0;n<snap.typeCount&&n<MaxTypes;++n)if(snap.types[n].kind==7&&snap.types[n].index==index){valid=true;break;}
  }
 }
 if(valid){
  if(right){bool queued=false;for(int n=0;n<policy.count;++n)if(policy.queued[n]==index)queued=true;valid=queued;}
  else{uint32_t f=get<uint32_t>(lastHouse+(policy.tab?0x53CC:0x53BC));valid=policy.slots&&get<int>(f+0x24)<=54;}
 }
 if(down){if(!valid)return false;
  uint32_t f=get<uint32_t>(lastHouse+(policy.tab?0x53CC:0x53BC));
  uint32_t type=get<uint32_t>(policy.parent+0x520);
  // A click on a manually paused current product must still resume natively.
  if(!right&&type&&get<int>(type+0xDF8)==index){
   bool paused=get<bool>(f+0x70)&&get<bool>(f+0x71);uint32_t strip=0x87F7E8+0x1544+policy.tab*3988;int count=get<int>(strip+84);
   for(int n=0;n<count&&n<75;++n){uint32_t c=strip+88+n*52;if(get<int>(c)==index&&get<int>(c+4)==7&&get<int>(c+0x10)==2)paused=true;}
   if(paused||get<int>(f+0x24)==54)return false;
  }
  reservationCapture={true,policy,index,x,y,int(client.right),int(client.bottom),frame.top,now,button,uint32_t((msg->wParam&MK_SHIFT)!=0)};
  swallowReservation(msg);return true;
 }
 if(valid&&DWORD(now-captured.tick)<=1500&&policy.match==captured.policy.match&&policy.house==captured.policy.house&&
    policy.parent==captured.policy.parent&&policy.parentID==captured.policy.parentID&&frame.tab==captured.policy.tab&&
    index==captured.index&&frame.top==captured.top&&client.right==captured.width&&client.bottom==captured.height&&
    std::abs(x-captured.x)<=4&&std::abs(y-captured.y)<=4){
  sh->reserveClick={now,match,lastHouse,policy.parent,policy.parentID,policy.tab,index,right?2u:1u,captured.all};
  MemoryBarrier();InterlockedIncrement((volatile LONG*)&sh->reserveInput);notifyHost();
 }
 swallowReservation(msg);return true;
}
