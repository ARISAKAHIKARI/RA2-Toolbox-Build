#pragma once
#include <cstdint>
#include <cstring>
#include <string>
namespace keygesture {
// Keep HOTKEYF_EXT (0x0800) separate from gestures and leave all wire layouts
// unchanged. Zero is deliberately contextual: old command keys mean Press,
// while old mouse-authorization keys mean Hold.
enum class Mode:uint16_t {Legacy=0,Press=0x1000,Release=0x2000,Hold=0x3000};
constexpr uint16_t Double=0x8000,Physical=0x7ff,ModeMask=0x3000,Stored=0xbfff;
constexpr uint32_t Window=350;
inline uint16_t physical(uint32_t binding){return uint16_t(binding&Physical);}
inline Mode mode(uint32_t binding){return Mode(binding&ModeMask);}
inline Mode effective(uint32_t binding,Mode fallback=Mode::Press){return mode(binding)==Mode::Legacy?fallback:mode(binding);}
inline uint16_t normalize(uint32_t binding,Mode fallback=Mode::Press){
 uint16_t value=uint16_t(binding&(Physical|ModeMask|Double));
 if(mode(value)==fallback)value&=uint16_t(~ModeMask);return value;
}
inline bool isHold(uint32_t binding,bool legacyHold=true){return effective(binding,legacyHold?Mode::Hold:Mode::Press)==Mode::Hold;}
inline bool sampledOnly(uint32_t binding){const auto key=physical(binding);unsigned vk=key&255;return (vk==16||vk==17||vk==18)||vk==1||vk==2||vk==4||vk==5||vk==6;}
template<class Keys> inline bool modifiersMatch(const Keys& keys,uint32_t binding){
 unsigned key=binding&255,mods=(binding>>8)&7;
 if(physical(binding)>=16&&physical(binding)<=18)return true;
 // A modifier used as the main key is not also a prefix modifier.
 return (key==16||bool(keys[16])==bool(mods&1))&&(key==17||bool(keys[17])==bool(mods&2))&&(key==18||bool(keys[18])==bool(mods&4));
}
template<class Keys> inline bool matchesHold(const Keys& keys,uint32_t binding){
 unsigned vk=binding&255;return vk&&isHold(binding)&&keys[vk]&&(!(binding&Double)||(keys[vk]&2))&&modifiersMatch(keys,binding);
}
inline const wchar_t* modeText(uint32_t binding,bool continuous=false){
 auto action=effective(binding,continuous?Mode::Hold:Mode::Press);bool twice=(binding&Double)!=0;
 return action==Mode::Hold?(twice?L"双击按住":L"持续按住"):action==Mode::Release?(twice?L"双击松开":L"单次松开"):(twice?L"双击按下":L"单次按下");
}
inline std::wstring describe(uint32_t binding,const std::wstring& name,bool continuous=false){
 if(!(binding&255))return L"未绑定";
 std::wstring prefix;unsigned mods=(binding>>8)&7,vk=binding&255;
 for(unsigned i=0;i<3;++i)if((mods&(1u<<i))&&vk!=16+i){if(!prefix.empty())prefix+=L"+";prefix+=i==0?L"Shift":i==1?L"Ctrl":L"Alt";}
 std::wstring text=prefix.empty()?L"":L"按住 "+prefix+L"，再";
 const auto action=effective(binding,continuous?Mode::Hold:Mode::Press);const bool twice=(binding&Double)!=0;
 if(twice)text+=L"双击 "+name+(action==Mode::Hold?L"，第二次保持按住（松开即结束）":action==Mode::Release?L"，第二次松开时触发":L"，第二次按下时触发");
 else if(action==Mode::Hold)text+=L"按住 "+name+L"（松开即结束）";
 else if(action==Mode::Release)text+=L"按下并松开 "+name+L"（松开时触发）";
 else text+=L"按下 "+name+L"（每次按下一次）";
 if(!prefix.empty())text+=L"；修饰键须保持按住";return text;
}
inline uint16_t chord(unsigned vk,const uint8_t*keys){unsigned mods=(keys[16]?1:0)|(keys[17]?2:0)|(keys[18]?4:0);if(vk==16)mods&=~1u;if(vk==17)mods&=~2u;if(vk==18)mods&=~4u;return uint16_t(vk|(mods<<8));}
struct Tracker {
 uint8_t held[256]{},pending[256]{};uint16_t chords[256]{};uint32_t at[256]{},serial[256]{};uint32_t next=0;
 void clear(){uint32_t sequence=next;*this={};next=sequence;}
 void edge(unsigned vk,bool down,bool repeat,uint32_t now,uint16_t code){
  if(vk>=256)return;if(!down){bool released=held[vk]!=0;held[vk]=0;
   if(released&&vk>=16&&vk<=18){const unsigned bit=1u<<(vk-16+8);for(unsigned i=0;i<256;++i)if(i!=vk&&held[i]&&(chords[i]&bit)){held[i]&=1;pending[i]=0;}}
   return;}if(repeat||held[vk])return;
  bool twice=pending[vk]&&chords[vk]==code&&uint32_t(now-at[vk])<=Window;
  // A different ordinary key interrupts a modifier double tap / key pair.
  if(vk!=16&&vk!=17&&vk!=18&&(vk<160||vk>165))for(unsigned i=0;i<256;++i)if(i!=vk)pending[i]=0;
  held[vk]=twice?3:1;pending[vk]=!twice;chords[vk]=code;at[vk]=now;serial[vk]=++next;
 }
 void sample(const uint8_t*raw,uint32_t now){for(unsigned i=0;i<256;++i)if(!raw[i])edge(i,false,false,now,0);for(unsigned i=0;i<256;++i)if(raw[i]){
  // Releasing a chord prefix cancels its double-hold qualification. Pressing
  // the prefix again must not resurrect an already held second tap.
  if(held[i]&&chords[i]!=chord(i,raw)){held[i]&=1;pending[i]=0;}
  edge(i,true,false,now,chord(i,raw));}}
 void decorate(uint8_t*keys)const{for(unsigned i=0;i<256;++i)if(keys[i]&&(held[i]&2))keys[i]|=2;}
 bool qualifies(uint16_t binding)const{return matchesHold(held,binding);}
};
}
