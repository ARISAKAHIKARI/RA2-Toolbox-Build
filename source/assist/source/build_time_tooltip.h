#pragma once
#include "../../shared/source/runtime_fp_scope.h"
#include "../../shared/source/native_tooltip_client.h"
#include "build_time_rate.h"
// Local presentation, through the existing entry manager. The original
// CCToolTip callback runs first; Process724AD0 measures/copies our text into
// the same native box. Never extend Sidebar's 66-wchar static buffer in place.
// Layout/ABI: YRpp d979554; synthetic query pattern: Phobos f06797e (GPL-3.0).
static bool buildTimeTooltipActive,buildTimeTooltipFailed;static uint32_t buildTimeTooltipFailedMatch;
extern "C" void AssistBuildTimeTooltipFailed(){buildTimeTooltipFailed=true;buildTimeTooltipFailedMatch=match;}
static void restoreBuildTimeTooltip();
static bool buildTimeTooltipCompatible(){
 // The fake object is allowed only for the verified read-only implementation.
 // Foreign tooltip/TimeToBuild detours can require additional fields.
 static const struct {unsigned at,n;uint8_t bytes[12];} probes[]={
  {0x479050,11,{0x8B,0x0D,0x40,0x76,0x88,0,0x8B,0x01,0xFF,0x50,0x28}},
  {0x6A9316,9,{0x8B,0x48,0x5C,0x83,0xF9,0x1F,0x75,0x13,0x8B}},
  {0x6F47A0,9,{0x83,0xEC,8,0x53,0x56,0x8B,0xF1,0x8B,0x06}},
  {0x6F4803,9,{0xE8,0x28,0x86,0xE0,0xFF,0xD9,0x5C,0x24,0x0C}},
  {0x4C9FB0,7,{0x8B,0x49,0x58,0x33,0xD2,0x85,0xC9}}
 };
 for(const auto&p:probes){uint8_t bytes[12]{};if(!read(p.at,bytes,p.n)||memcmp(bytes,p.bytes,p.n))return false;}
 return true;
}
static void buildTimeTooltipTick(){
 if(!sh||!sh->buildTimeTooltip||buildTimeTooltipFailedMatch!=match)buildTimeTooltipFailed=false;
 if(buildTimeTooltipFailed){restoreBuildTimeTooltip();if(sh)sh->buildTimeTooltipStatus=-3;return;}
 // Focus controls callback work, never ownership of the installed slot.
 const bool wanted=sh&&!sh->stop&&sh->buildTimeTooltip&&!fault&&snap.status==1;
 if(!wanted){restoreBuildTimeTooltip();return;}
 if(buildTimeTooltipActive)return;
 if(!buildTimeTooltipCompatible()){sh->buildTimeTooltipStatus=-2;return;}
 buildTimeTooltipActive=NativeTooltipBuildTime(true);
 sh->buildTimeTooltipStatus=buildTimeTooltipActive?1:-2;
}
static void restoreBuildTimeTooltip(){
 // Release only this feature's claim. The cursor can still need the same
 // slot with the build-time option off, including after Assist disconnects.
 if(!NativeTooltipBuildTime(false)){
  buildTimeTooltipActive=true;if(sh)sh->buildTimeTooltipStatus=-2;return;
 }
 buildTimeTooltipActive=false;
 if(sh)sh->buildTimeTooltipStatus=0;
}
// No gameplay object is constructed, registered, charged or ticked. Only the
// verified native TimeToBuild reads this scratch receiver's Type and Owner.
extern "C" unsigned AssistBuildTimeTooltip(unsigned id,unsigned original){
 if(!sh||sh->stop||!sh->buildTimeTooltip||!sh->foreground||fault||buildTimeTooltipFailed||!buildTimeTooltipActive||
    DWORD(sampleClock(sh)-sh->heartbeat)>500||!get<bool>(0xA8E9A0)||id<1000||id>=1120)return original;
 const uint32_t house=get<uint32_t>(0xA83D4C);const int frame=get<int>(0xA8ED84);
 if(house<0x10000||house!=lastHouse||snap.match!=match)return original;
 const int tab=get<int>(0x87F7E8+0x539C);
 if(tab<0||tab>1)return original; // buildings/defenses only
 const uint32_t strip=0x87F7E8+0x1544+uint32_t(tab)*3988;
 const int row=get<int>(strip+0x44),count=get<int>(strip+0x54);
 const int64_t index=int64_t(row)*2+int(id-1000);
 if(count<1||count>=75||index<0||index>=count)return original;
 const uint32_t cameo=strip+88+uint32_t(index)*52;
 if(get<int>(cameo+4)!=7)return original;
 const int typeIndex=get<int>(cameo);Vec types=vec(0xA83C68,4096);
 const uint32_t type=item(types,typeIndex);if(type<0x10000)return original;
 alignas(16) uint8_t receiver[0x6C8]{};
 *reinterpret_cast<uint32_t*>(receiver)=0x7E3EBC;
 *reinterpret_cast<uint32_t*>(receiver+0x520)=type;
 *reinterpret_cast<uint32_t*>(receiver+0x21C)=house;
 const int raw=call<int>(0x6F47A0,uint32_t(uintptr_t(receiver)));
 int rate=raw/54;if(rate<1)rate=1;if(rate>255)rate=255;
 // 54 simulation steps; one frame for the sidebar's ready handoff. Do not
 // mistake Select6AB5CC's rate+1 presentation clock for 54 extra frames.
 const int total=54*rate+1;
 const int session=get<int>(0xA8B238);
 // Use the selected speed's maximum, not measured/adaptively reduced FPS.
 // Single-player unlimited speed 6 is estimated at speed 5 by user request.
 int speed=-1;read(0xA8EB60,&speed,sizeof speed);
 const unsigned custom=singlefps::activeRate();
 const unsigned fps=custom?custom:buildtime::allowedRate(session,speed);
 static wchar_t text[256];unsigned used=0;
 // Process724AD0 copies at most 256 UTF-16 units without adding a NUL.
 // Refuse rather than truncate a translated name or another extension's text.
 for(;used<180;++used){uint16_t c=0;if(!read(original+used*2,&c,2))return original;text[used]=wchar_t(c);if(!c)break;}
 if(used>=180||!used)return original;
 auto append=[&](const wchar_t*s){while(*s&&used<255)text[used++]=*s++;text[used]=0;};
 auto number=[&](unsigned n){wchar_t digits[12];unsigned k=0;do{digits[k++]=wchar_t(L'0'+n%10);n/=10;}while(n&&k<12);while(k){wchar_t pair[2]={digits[--k],0};append(pair);}};
 auto duration=[&](unsigned frames){if(fps){const unsigned centis=unsigned((uint64_t(frames)*100+fps/2)/fps);number(centis/100);append(L".");number(centis%100/10);number(centis%10);append(L" 秒");}else{number(frames);append(L" 帧");}};
 append(L"\n建造：");duration(unsigned(total));
 const uint32_t factory=get<uint32_t>(cameo+0xC);
 if(factory>=0x10000&&get<uint32_t>(factory+0x6C)==house){
  const uint32_t object=get<uint32_t>(factory+0x58);
  if(object>=0x10000&&kind(object)==3&&get<uint32_t>(object+0x520)==type){
   const int stage=get<int>(factory+0x24);if(stage>=0&&stage<=54){
    if(stage==54)append(L"\n已就绪");
    else if(get<uint8_t>(factory+0x70))append(L"\n已暂停");
    else if(get<uint8_t>(factory+0x5C))append(L"\n等待资金");
    else {const int stored=get<int>(factory+0x38),durationFrames=get<int>(factory+0x34),start=get<int>(factory+0x2C);
     if(stored>=1&&stored<=255&&durationFrames>=0&&durationFrames<=255&&start>=0&&start<=frame){
      int wait=durationFrames-(frame-start);if(wait<0)wait=0;
      // House4CA6E0 changes +38 only: retain this step's original deadline.
      append(L"\n剩余：");duration(unsigned(wait+(53-stage)*stored+1));
     }
    }
   }
  }
 }
 return get<uint32_t>(0xA83D4C)==house&&get<int>(0xA8ED84)==frame&&get<int>(cameo)==typeIndex?unsigned(uintptr_t(text)):original;
}
