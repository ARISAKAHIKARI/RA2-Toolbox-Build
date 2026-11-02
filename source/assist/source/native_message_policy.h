#include "construction_message_bridge.h"
// Called only by GameRuntime's existing WH_GETMESSAGE manager, never a new hook.
static uint32_t blockedButtons;
static pointerinput::SuppressedClick blockedOrigins[2];
static void recordRuntimeGesture(const MSG*msg,bool keyContext,uint32_t epoch,uint32_t now){
 static uint32_t keyEpoch=0;
 if(!keyContext||keyEpoch!=epoch){runtimeKeys.clear();keyEpoch=epoch;}
 if(keyContext){
  unsigned vk=0;bool down=false,repeat=false;
  switch(msg->message){
   case WM_KEYDOWN:case WM_SYSKEYDOWN:vk=unsigned(msg->wParam)&255;down=true;repeat=(msg->lParam&(1u<<30))!=0;break;
   case WM_KEYUP:case WM_SYSKEYUP:vk=unsigned(msg->wParam)&255;break;
   case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:vk=VK_LBUTTON;down=true;break;case WM_LBUTTONUP:vk=VK_LBUTTON;break;
   case WM_RBUTTONDOWN:case WM_RBUTTONDBLCLK:vk=VK_RBUTTON;down=true;break;case WM_RBUTTONUP:vk=VK_RBUTTON;break;
   case WM_MBUTTONDOWN:case WM_MBUTTONDBLCLK:vk=VK_MBUTTON;down=true;break;case WM_MBUTTONUP:vk=VK_MBUTTON;break;
   case WM_XBUTTONDOWN:case WM_XBUTTONDBLCLK:vk=HIWORD(msg->wParam)==XBUTTON1?VK_XBUTTON1:HIWORD(msg->wParam)==XBUTTON2?VK_XBUTTON2:0;down=true;break;
   case WM_XBUTTONUP:vk=HIWORD(msg->wParam)==XBUTTON1?VK_XBUTTON1:HIWORD(msg->wParam)==XBUTTON2?VK_XBUTTON2:0;break;
  }
  if(vk){uint8_t physical[256]{};for(unsigned modifier:{16u,17u,18u})physical[modifier]=(GetKeyState(modifier)&0x8000)!=0;
   runtimeKeys.edge(vk,down,repeat,now,keygesture::chord(vk,physical));}
 }
}
extern "C" int AssistFilterMessage(void* raw){
 auto*msg=static_cast<MSG*>(raw);if(!msg)return 0;
 bool keyContext=sh&&!sh->stop&&!fault&&sh->foreground&&!get<bool>(0xA8BC60+25)&&DWORD(sampleClock(sh)-sh->heartbeat)<=150;
 recordRuntimeGesture(msg,keyContext,match,sampleClock(sh));
 if(moOrdersMessage(msg))return 1;
 if(formationMessage(msg))return 1;
 if(reservationMessage(msg))return 1;
 bool left=msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONUP||msg->message==WM_LBUTTONDBLCLK;
 bool right=msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONUP||msg->message==WM_RBUTTONDBLCLK;
 if(!left&&!right)return 0;
 bool up=msg->message==WM_LBUTTONUP||msg->message==WM_RBUTTONUP;
 uint32_t bit=left?1:2,physical=left?6:4;int slot=left?0:1;
 if(up){
  if(!(blockedButtons&bit))return 0;blockedButtons&=~bit;
  if(sh){auto record=blockedOrigins[slot];record.tick=sampleClock(sh);sh->suppressedClick=record;}
  msg->message=WM_NULL;msg->wParam=msg->lParam=0;return 1;
 }
 if(!sh||fault||sh->stop||!nativeReady()||!sh->foreground||DWORD(sampleClock(sh)-sh->heartbeat)>150)return 0;
 DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground!=GetCurrentProcessId())return 0;
 auto seq=sh->pointerPolicySeq;if(seq&1)return 0;pointerinput::NativePolicy policy;MemoryBarrier();memcpy(&policy,&sh->pointerPolicy,sizeof policy);MemoryBarrier();
 if(seq!=sh->pointerPolicySeq||!policy.enabled||!policy.overrideNative||policy.match!=match)return 0;
 uint8_t keys[256]{};for(unsigned n=0;n<256;++n)keys[n]=(GetKeyState(n)&0x8000)!=0;runtimeKeys.decorate(keys);
 auto route=pointerinput::resolveMouse(keys,physical,policy.buttons,policy.bindings,0);
 if(route.operation!=pointerinput::Enhanced&&route.operation!=pointerinput::AttackMove)return 0;
 const bool singleCooperative=cooperativeSingleInput(route.bindings);
 if((route.bindings&pointerinput::Shift)&&!singleCooperative)return 0;
 if((msg->message==WM_LBUTTONDBLCLK||msg->message==WM_RBUTTONDBLCLK)&&!singleCooperative)return 0;
 // Limited single-target attacks must fail closed when the tool queue/mailbox
 // is busy: forwarding the physical click would order EVERY selected plane.
 if(!singleCooperative&&(sh->pointer.request!=sh->pointer.ack||outgoingRoom()<=0))return 0;
 // Object clicks, dragging a build placement, sidebar/minimap, unsupported
 // selections and absent host capacity keep their original native message.
 uint32_t tactical=get<uint32_t>(0x887324);if(tactical<0x10000||get<bool>(0x87F7E8+0x11B3))return 0;
 pointerinput::Intent point{};point.width=get<int>(0x8A00A4);point.height=get<int>(0x8A00A8);point.cameraX=get<int>(tactical+0xB0);point.cameraY=get<int>(tactical+0xB4);
 RECT client{};if(!GetClientRect(msg->hwnd,&client)||client.right<=0||client.bottom<=0)return 0;
 point.x=int(int64_t(short(LOWORD(msg->lParam)))*point.width/client.right);point.y=int(int64_t(short(HIWORD(msg->lParam)))*point.height/client.bottom);
 uint32_t oldCell=pointerCell,oldTarget=pointerTarget,oldID=pointerTargetID;int oldZ=pointerGroundZ;bool location=pointerLocation(point);bool ground=location&&!pointerTarget;
 bool singleObject=singleCooperative&&location&&pointerTarget&&nativeEnemy(pointerTarget,lastHouse);
 pointerCell=oldCell;pointerTarget=oldTarget;pointerTargetID=oldID;pointerGroundZ=oldZ;if(singleCooperative?!singleObject:!ground)return 0;
 Vec selected=vec(0xA8ECB8);if(selected.count<=0||(!singleObject&&selected.count>pointerinput::MaxSources))return 0;
 bool selectedAircraft=false;
 for(int n=0;n<selected.count;++n){uint32_t p=item(selected,n);if(!p||!worldObjects.contains(p)||!own(p,lastHouse))return 0;
  if(singleObject){int k=kind(p);selectedAircraft|=k==0||(k==2&&(get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D)));continue;}
  if(!rawArmed(p)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||(kind(p)!=1&&kind(p)!=2))return 0;
  bool supported=route.operation==pointerinput::AttackMove||
   ((route.bindings&pointerinput::Advanced)&&(sh->flags&AdvancedInput)&&featureObjectAllowed(featuretypes::AdvancedAttack,p))||
   ((route.bindings&pointerinput::Magnetron)&&(sh->flags&MagnetronInput)&&rawMagnetron(p))||
   ((route.bindings&pointerinput::CS)&&csAllowedFor(p,true));
  if(!supported)return 0;
 }
 if(singleObject&&!selectedAircraft)return 0;
 blockedButtons|=bit;blockedOrigins[slot]={match,sampleClock(sh),physical,point.x,point.y};sh->suppressedClick=blockedOrigins[slot];
 msg->message=WM_NULL;msg->wParam=msg->lParam=0;return 1;
}
extern "C" int GuardedAssistFilter(void*);
extern "C" void AssistRuntimeFilter(void* message){
 gameentry::Scope entry(gameentry::Client::Assist);if(!entry)return;
 GuardedAssistFilter(message);
}

