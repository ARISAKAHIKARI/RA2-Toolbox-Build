#pragma once
#include "native_feedback.h"
#include <array>
#include <map>
#include <string>
namespace external {namespace localui {
// GAME.FNT glyphs currently loaded by the game, copied as immutable values.
// Format/layout reference: YuriPlanet/vera20k 0b6a073, assets/fnt_file.rs and
// render/bit_font.rs (GPL-3.0). No font asset is bundled with the toolbox.
struct Glyph {int width=0,rows=0;bool missing=false;std::vector<uint8_t>ink;};
class BitmapFont {
 struct Identity {uint32_t font=0,data=0;int line=0;std::array<uint32_t,9>fields{};
  bool operator==(const Identity&o)const{return font==o.font&&data==o.data&&line==o.line&&fields==o.fields;}
 }identity_;
 std::vector<uint16_t>lookup_;std::map<uint16_t,Glyph>glyphs_;uint64_t captures_=0,decodes_=0;size_t bytes_=0;
 template<class Read>static bool identity(Read read,Identity&v){
  if(!read(0x89C4D0,&v.font,4)||!nativefeedback::range(v.font,0x24)||!read(v.font+4,&v.data,4)||!nativefeedback::range(v.data,36)||!read(v.data,v.fields.data(),36)||!read(v.font+0x1C,&v.line,4))return false;
  const auto&f=v.fields;return f[0]>0&&f[0]<=256&&f[1]>0&&f[1]<=32&&f[2]>0&&f[2]<=128&&f[3]>0&&f[3]<=256&&f[4]>0&&f[4]<=65536&&f[5]>=1+f[1]*f[2]&&f[5]<=8192&&v.line>0&&v.line<=256&&nativefeedback::range(f[6],65536*2)&&nativefeedback::range(f[7],size_t(f[4])*f[5]);
 }
public:
 void reset(){identity_={};lookup_.clear();glyphs_.clear();bytes_=0;}
 int lineHeight()const{return identity_.line;}
 int rows()const{return int(identity_.fields[2]);}
 uint64_t captures()const{return captures_;}uint64_t decodes()const{return decodes_;}
 template<class Read>bool begin(Read read){Identity next;if(!identity(read,next)){reset();return false;}
  if(!lookup_.empty()&&next==identity_)return true;
  std::vector<uint16_t>table(65536);Identity after;
  if(!read(next.fields[6],table.data(),table.size()*2)||!identity(read,after)||!(after==next)){reset();return false;}
  identity_=next;lookup_=std::move(table);glyphs_.clear();bytes_=0;++captures_;return true;
 }
 template<class Read>bool stable(Read read)const{Identity now;return !lookup_.empty()&&identity(read,now)&&now==identity_;}
 template<class Read>const Glyph*glyph(Read read,uint16_t ch){
  auto found=glyphs_.find(ch);if(found!=glyphs_.end())return &found->second;if(lookup_.empty())return nullptr;
  bool missing=lookup_[ch]==0;uint32_t slot=lookup_[missing?0xB0:ch];Glyph g;g.rows=rows();g.missing=missing;
  // Space advances by its authored width but never contributes ink, even
  // when a malformed/missing space would otherwise select the degree glyph.
  if(ch==' '){uint8_t width=0;if(lookup_[ch]&&(lookup_[ch]>identity_.fields[4]||!read(identity_.fields[7]+(lookup_[ch]-1)*identity_.fields[5],&width,1)||width>identity_.fields[1]*8))return nullptr;g.width=width?width:4;g.missing=false;g.ink.assign(size_t(g.width)*g.rows,0);auto p=glyphs_.emplace(ch,std::move(g));return &p.first->second;}
  if(!slot)return nullptr;
  if(slot>identity_.fields[4])return nullptr;
  const auto stride=identity_.fields[5];std::vector<uint8_t>data(stride);uint32_t at=identity_.fields[7]+(slot-1)*stride;
  if(!read(at,data.data(),data.size())||data[0]>identity_.fields[1]*8)return nullptr;
  if(!data[0])return nullptr;
  g.width=data[0];g.ink.resize(size_t(g.width)*g.rows);for(int y=0;y<g.rows;++y)for(int x=0;x<g.width;++x){bool on=(data[1+y*identity_.fields[1]+x/8]&(0x80u>>(x%8)))!=0;g.ink[size_t(y)*g.width+x]=uint8_t(missing?!on:on);}
  if(bytes_+g.ink.size()>16*1024*1024||glyphs_.size()>=4096){glyphs_.clear();bytes_=0;}bytes_+=g.ink.size();++decodes_;auto p=glyphs_.emplace(ch,std::move(g));return &p.first->second;
 }
 template<class Read>bool measure(Read read,const std::u16string&text,int&width){int64_t value=0;for(auto ch:text){if(ch=='\r'||ch=='\n')continue;if(ch=='\t'){value=(value+64)/64*64;continue;}auto*g=glyph(read,ch);if(!g)return false;value+=g->width+1;if(value>1048576)return false;}width=int(value);return true;}
};
template<class Read>bool string(Read read,uint32_t pointer,std::u16string&out){
 out.clear();if(!nativefeedback::range(pointer,8192))return false;
 for(unsigned at=0;at<4096;at+=16){std::array<uint16_t,16>chars{};if(read(pointer+at*2,chars.data(),sizeof chars)){for(auto c:chars){if(!c)return true;out.push_back(char16_t(c));}}else for(unsigned i=0;i<16;++i){uint16_t c=0;if(!read(pointer+(at+i)*2,&c,2))return false;if(!c)return true;out.push_back(char16_t(c));}}
 out.clear();return false;
}
class Strings {
 std::array<uint32_t,4>identity_{};std::map<std::string,int>indices_;
public:
 void reset(){identity_={};indices_.clear();}
 template<class Read>bool lookup(Read read,const char*name,std::u16string&out){
  std::array<uint32_t,4>head{},after{};if(!read(0xB1CF6C,head.data(),sizeof head)||!head[0]||head[0]>131072||!head[1]||head[1]>262144||!nativefeedback::range(head[2],size_t(head[0])*40)||!nativefeedback::range(head[3],size_t(head[1])*4))return false;
  if(head!=identity_){std::vector<uint8_t>labels(size_t(head[0])*40);if(!read(head[2],labels.data(),labels.size())||!read(0xB1CF6C,after.data(),sizeof after)||head!=after)return false;
   std::map<std::string,int>index;for(size_t i=0;i<head[0];++i){const auto*p=labels.data()+40*i;size_t n=0;while(n<32&&p[n])++n;if(n==32)continue;int values=int32_t(nativefeedback::u32(p+32)),first=int32_t(nativefeedback::u32(p+36));if(values<=0||first<0||uint64_t(first)>=head[1])continue;std::string key(reinterpret_cast<const char*>(p),n);for(auto&c:key)if(c>='a'&&c<='z')c-=32;index[key]=first;}
   indices_=std::move(index);identity_=head;
  }
  std::string key=name;for(auto&c:key)if(c>='a'&&c<='z')c-=32;auto at=indices_.find(key);if(at==indices_.end()){out=u"MISSING:";for(char c:key)out.push_back(char16_t(c));return true;}
  uint32_t value=0;if(!read(head[3]+uint32_t(at->second)*4,&value,4)||!string(read,value,out)||!read(0xB1CF6C,after.data(),sizeof after)||head!=after)return false;return true;
 }
};
inline uint32_t rgb565(uint32_t color){unsigned r=(color>>16)&255,g=(color>>8)&255,b=color&255;r>>=3;g>>=2;b>>=3;return 0xFF000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}
inline uint32_t missingColor(uint32_t color){uint32_t word=(((color>>16)&255)>>3)<<11|(((color>>8)&255)>>2)<<5|(color&255)>>3;word^=0x5555;unsigned r=word>>11&31,g=word>>5&63,b=word&31;return 0xFF000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}
}}
