#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include "native_surface.h"
namespace external { namespace lighting {
// Raw palettes contain expanded RGB, not a rendered lighting ramp. Read the
// current ambient and LightConvert tint separately; never call the renderer.
// Native offsets/units and special-light transitions: research pre.12.
struct State {
 int ambient=100;std::array<int,3> tint{{1000,1000,1000}};
 int intensity=-1; // Native cell intensity already includes Ambient/Ground/Level.
 unsigned paletteRows=53;bool scalar=false,rgb555=false;std::shared_ptr<const std::vector<uint32_t>>table;
 bool operator==(const State&o)const{return ambient==o.ambient&&tint==o.tint&&intensity==o.intensity&&paletteRows==o.paletteRows&&scalar==o.scalar&&rgb555==o.rgb555&&table==o.table;}
 bool operator!=(const State&o)const{return !(*this==o);}
 std::array<float,3> gains()const{int value=intensity<0?ambient*10:intensity;return {{value*tint[0]/1000000.f,value*tint[1]/1000000.f,value*tint[2]/1000000.f}};}
};
template<class Read>void converterMetadata(Read read,uint32_t convert,State&out){
 int rows=0;uint32_t vt=0;std::array<int,6>format{},after{};out.table.reset();
 if(read(0x8A0DD0,format.data(),sizeof format)&&read(0x8A0DD0,after.data(),sizeof after)&&format==after)out.rgb555=format==std::array<int,6>{{10,3,0,3,5,3}};
 if(convert<0x10000||convert>0xfffffe90u||!read(convert+0x16C,&rows,4)||rows<1||rows>256||!read(convert,&vt,4)||(vt!=0x7E5358&&vt!=0x7ED0A4))return;
 out.paletteRows=unsigned(rows);out.scalar=vt==0x7E5358;
 // FullColorData contains the evaluated native palette rows. Capture one
 // immutable table per converter and committed game frame, retaining pointer
 // identity when its content is unchanged. No neutral-row reconstruction.
 struct Entry{int frame=-1,rows=0;uint32_t data=0;std::shared_ptr<const std::vector<uint32_t>>table;};static thread_local std::unordered_map<uint32_t,Entry>cache;static thread_local uint32_t scenario=0;
 uint32_t map=0,data=0;int frame=-1,bpp=0;read(0xA8B230,&map,4);read(0xA8ED84,&frame,4);if(scenario!=map||cache.size()>1024){cache.clear();scenario=map;}
 if(!read(convert+4,&bpp,4)||bpp!=2||!read(convert+0x170,&data,4)||data<0x10000||uint64_t(data)+uint64_t(rows)*512>0x100000000ull)return;
 auto&entry=cache[convert];if(frame>=0&&entry.frame==frame&&entry.rows==rows&&entry.data==data&&entry.table){out.table=entry.table;return;}
 std::vector<uint16_t>words(size_t(rows)*256);int rowsAfter=0;uint32_t dataAfter=0;if(!read(data,words.data(),words.size()*2)||!read(convert+0x170,&dataAfter,4)||dataAfter!=data||!read(convert+0x16C,&rowsAfter,4)||rowsAfter!=rows)return;
 bool same=entry.table&&entry.table->size()==words.size();if(same)for(size_t n=0;n<words.size();++n)if((*entry.table)[n]!=words[n]){same=false;break;}
 if(!same)entry.table=std::make_shared<const std::vector<uint32_t>>(words.begin(),words.end());entry.frame=frame;entry.rows=rows;entry.data=data;out.table=entry.table;
}

inline nativesurface::Palette palette(const State&s,bool object){nativesurface::Palette p;p.rows=s.paletteRows;p.brightness=s.intensity<0?s.ambient*10:s.intensity;
 for(int n=0;n<3;++n)p.scale[n]=s.scalar?65536:nativesurface::scale16(s.tint[n]);p.special=object;p.scalar=s.scalar;p.rgb555=s.rgb555;p.table=s.table;return p;}
template<class Read>bool converterTint(Read read,uint32_t convert,std::array<int,3>&out){
 if(convert<0x10000||convert>0xfffffe00)return false;
 uint8_t tinted=0;if(!read(convert+0x1B0,&tinted,1)||tinted>1)return false;
 uint32_t at=convert+(tinted?0x1A4:0x198);std::array<int,3> current{},after{};
 if(!read(at,current.data(),12)||std::any_of(current.begin(),current.end(),[](int x){return x<0||x>2000;}))return false;
 uint8_t again=2;if(!read(at,after.data(),12)||after!=current||!read(convert+0x1B0,&again,1)||again!=tinted)return false;
 out=current;return true;
}
// One presentation/read epoch only. Thousands of cells usually share the
// same converter; validate its tint once rather than five reads per cell.
// Cache failures as failures, so callers retain their own fallback tint.
struct TintCache {
 struct Entry {std::array<int,3> tint{};bool valid=false,metadataReady=false;
  unsigned inputRows=0,rows=0;bool inputScalar=false,input555=false,scalar=false,rgb555=false;
  std::shared_ptr<const std::vector<uint32_t>>table;
 };
 std::unordered_map<uint32_t,Entry> entries;
 template<class Read>bool sample(Read read,uint32_t convert,std::array<int,3>&out){
  auto found=entries.find(convert);
  if(found==entries.end()){Entry e;e.valid=converterTint(read,convert,e.tint);found=entries.emplace(convert,e).first;}
  if(!found->second.valid)return false;out=found->second.tint;return true;
 }
 template<class Read>void shade(Read read,uint32_t convert,State&out){
  sample(read,convert,out.tint);auto&e=entries.find(convert)->second;
  // Metadata is shared by the same converter during this presentation too.
  // Its failure paths preserve some incoming fields, so include those inputs
  // rather than caching a complete State (cell intensity remains individual).
  if(!e.metadataReady||e.inputRows!=out.paletteRows||e.inputScalar!=out.scalar||e.input555!=out.rgb555){
   e.inputRows=out.paletteRows;e.inputScalar=out.scalar;e.input555=out.rgb555;
   converterMetadata(read,convert,out);e.rows=out.paletteRows;e.scalar=out.scalar;e.rgb555=out.rgb555;e.table=out.table;e.metadataReady=true;
  }else{out.paletteRows=e.rows;out.scalar=e.scalar;out.rgb555=e.rgb555;out.table=e.table;}
 }
};
// 705D70: ordinary SHP and VXL use their owner's ColorScheme, including a
// type-specific CustomPalette vector. A cell's LightConvert is terrain art;
// its RGB tint must not leak onto these objects.
template<class Read>uint32_t schemeConverter(Read read,uint32_t vector,int index){
 if(vector<0x10000||vector>0xffffffebu||index<0||index>=512)return 0;
 uint32_t items=0,scheme=0,convert=0,again=0;int count=0,after=0;
 if(!read(vector+4,&items,4)||!read(vector+16,&count,4)||count<=index||count>512||items<0x10000||
    uint64_t(items)+uint64_t(count)*4>0x100000000ull||!read(items+uint32_t(index)*4,&scheme,4)||scheme<0x10000||scheme>0xfffffcefu||
    !read(scheme+0x30C,&convert,4)||convert<0x10000||convert>0xfffffe4fu)return 0;
 if(!read(vector+4,&again,4)||again!=items||!read(vector+16,&after,4)||after!=count||
    !read(items+uint32_t(index)*4,&again,4)||again!=scheme||!read(scheme+0x30C,&again,4)||again!=convert)return 0;
 return convert;
}
template<class Read>uint32_t objectConverter(Read read,uint32_t type,uint32_t owner){
 if(type<0x10000||type>0xfffff20bu||owner<0x10000||owner>0xfffe9fa7u)return 0;
 uint32_t vector=0,after=0;int index=-1,again=-1;
 if(!read(type+0xDF0,&vector,4)||!read(owner+0x16054,&index,4))return 0;
 uint32_t convert=schemeConverter(read,vector?vector:0xB054D0,index);
 if(!read(type+0xDF0,&after,4)||after!=vector||!read(owner+0x16054,&again,4)||again!=index)return 0;
 return convert;
}
template<class Read>State sample(Read read){
 State out;auto word=[&](uint32_t p){uint32_t v=0;read(p,&v,4);return v;};
 uint32_t scenario=word(0xA8B230);if(scenario<0x10000||scenario>0xffffc000)return out;
 int ambient=0;std::array<int,3> normal{};
 if(!read(scenario+0x352C,&ambient,4)||ambient<0||ambient>300||!read(scenario+0x3534,normal.data(),12))return out;
 if(std::any_of(normal.begin(),normal.end(),[](int x){return x<0||x>200;}))return out;
 out.ambient=ambient;for(int n=0;n<3;++n)out.tint[n]=normal[n]*10;
 uint32_t items=word(0xB054D4);int count=int(word(0xB054E0));
 if(items>=0x10000&&count>0&&count<=512&&uint64_t(items)+count*4<=0x100000000ull){
  for(int n=0;n<count;++n){uint32_t scheme=word(items+n*4);if(scheme<0x10000||scheme>0xfffffc00||word(scheme+0x310)<=1)continue;
   if(converterTint(read,word(scheme+0x30C),out.tint))break;
  }
 }
 // Never publish a cross-map sample or a partially changing ambient value.
 int after=0;if(word(0xA8B230)!=scenario||!read(scenario+0x352C,&after,4)||after!=ambient)return {};
 return out;
}
enum class Layer {Object,Terrain,Bridge};
// Read the engine's evaluated cell, not a second approximation of [Lighting].
// 484680 combines height, Ground and current ambient (including special modes),
// then applies the slope/light-source intensity for terrain and bridge layers.
template<class Read>State cell(Read read,uint32_t p,const State&fallback,Layer layer=Layer::Object,bool localTint=true,TintCache*tints=nullptr){
 if(p<0x10000||p>0xfffffe00)return fallback;
 uint32_t scale=0,convert=0;int16_t values[3]{},after[3]{};
 if(!read(p+0x104,&scale,4)||scale>262144||!read(p+0x34,&convert,4)||(!scale&&convert<0x10000)||
    !read(p+0x10A,values,6)||std::any_of(values,values+3,[](int x){return x<0||x>2000;}))return fallback;
 State out=fallback;out.intensity=values[int(layer)];if(localTint){if(tints)tints->shade(read,convert,out);else{converterTint(read,convert,out.tint);converterMetadata(read,convert,out);}}
 if(!read(p+0x10A,after,6)||memcmp(values,after,6))return fallback;
 return out;
}
template<class Read>uint32_t cellAt(Read read,int x,int y){
 if(x<0||y<0||x>=512*256||y>=512*256)return 0;
 int cx=x/256,cy=y/256,index=cy*512+cx,count=0;uint32_t items=0,p=0;
 if(!read(0x87F7E8+0x13C,&items,4)||items<0x10000||!read(0x87F7E8+0x140,&count,4)||count<=index||count>512*512||uint64_t(items)+index*4+4>0x100000000ull||!read(items+index*4,&p,4)||p<0x10000||p>0xfffffe00)return 0;
 int16_t coordinates[2]{};return read(p+0x24,coordinates,4)&&coordinates[0]==cx&&coordinates[1]==cy?p:0;
}
template<class Read>State at(Read read,int x,int y,int z,const State&fallback,bool terrain=false,bool localTint=true){
 uint32_t p=cellAt(read,x,y);Layer layer=terrain?Layer::Terrain:Layer::Object;
 uint32_t flags=0;int8_t level=0;int height=0;
 if(!terrain&&p&&read(p+0x140,&flags,4)&&(flags&0x80)&&read(p+0x11B,&level,1)&&read(0x89E7C0,&height,4)&&height>0&&height<=1024&&z>=(int(level)+4)*height-1)layer=Layer::Bridge;
 return cell(read,p,fallback,layer,localTint);
}
template<class Read>bool spawnsTiberium(Read read,uint32_t type){
 uint8_t value=0;return type>=0x10000&&type<=0xfffffd4eu&&read(type+0x2B1,&value,1)&&value;
}
// 71C280/71C40F: ore-producing terrain uses the global resource drawer and
// object intensity. Ordinary trees use the cell's ISO drawer/terrain intensity.
// The original resource drawer is a plain ConvertClass, with no RGB tint.
template<class Read>State terrainObject(Read read,uint32_t type,int x,int y,int z,const State&fallback){
 if(!spawnsTiberium(read,type))return at(read,x,y,z,fallback,true);
 State out=cell(read,cellAt(read,x,y),fallback,Layer::Object,false);out.tint={{1000,1000,1000}};
 uint32_t convert=0,vtable=0;int shades=0;
 if(read(0x87F6BC,&convert,4)&&convert>=0x10000&&convert<=0xfffffe4fu){
  if(read(convert,&vtable,4)&&vtable==0x7ED0A4)converterTint(read,convert,out.tint);
  if(read(convert+0x16C,&shades,4)&&shades==1)out.intensity=1000;
 }
 return out;
}
template<class Read>State object(Read read,uint32_t type,uint32_t owner,int kind,int x,int y,int z,const State&fallback){
 uint8_t terrainPalette=0,voxel=0;
 if(type>=0x10000&&type<=0xfffffdc9u)read(type+0x236,&voxel,1);
 if(kind==3&&!voxel&&type>=0x10000&&type<=0xffffe8fcu&&read(type+0x1702,&terrainPalette,1)&&terrainPalette)
  return at(read,x,y,z,fallback,true); // 705EC1: also replaces ExtraLight with cell terrain intensity.
 State out=at(read,x,y,z,fallback,false,false);
 uint32_t converter=objectConverter(read,type,owner);converterTint(read,converter,out.tint);converterMetadata(read,converter,out);
 if(kind>=0&&kind<=2){
  // Unit::Draw 73D0C3 adds ExtraUnitLight before selecting VXL or SHP.
  // Infantry::Draw 519457 and Aircraft::Draw 414925 use separate fields.
  // Rules parsing stores config*1000 as signed integers, despite public
  // float declarations. Never infer one class's light from another class.
  uint32_t rules=0,after=0,offset=kind==2?0x17D4:(kind==1?0x17D8:0x17DC);int32_t extra=0;
  if(read(0x8871E0,&rules,4)&&rules>=0x10000&&uint64_t(rules)+offset+4<=0x100000000ull&&read(rules+offset,&extra,4)&&
     read(0x8871E0,&after,4)&&after==rules){
   int64_t base=out.intensity<0?int64_t(out.ambient)*10:out.intensity;
   out.intensity=int(std::clamp<int64_t>(base+extra,0,2000)); // presentation range; negative is not fallback ambient
  }
 }
 if(kind==3&&type>=0x10000&&type<=0xffffeab6u){int16_t extra=0;if(read(type+0x1548,&extra,2))out.intensity=(out.intensity<0?out.ambient*10:out.intensity)+extra;}
 return out;
}
struct AnimationLightSource {uint32_t converter=0;Layer layer=Layer::Object;bool ownIntensity=false,tintable=false;};
// 423114..423358. Building animations already retain the selected converter
// and brightness in +D4/+FC. UseNormalLight changes intensity to 1000; it does
// not discard the chosen converter's RGB (including temporary map lighting).
template<class Read>AnimationLightSource animationSource(Read read,uint32_t object,uint32_t type,uint32_t cellPointer){
 AnimationLightSource out;if(object<0x10000||object>0xfffffe68u||type<0x10000||type>0xfffffc9du)return out;
 uint8_t veins=0,useCell=0,alt=0;read(type+0x355,&veins,1);read(object+0x196,&useCell,1);read(type+0x361,&alt,1);
 if(veins){uint32_t owner=0;int index=-1;out.layer=Layer::Terrain;out.tintable=true;
  if(read(0xA83D4C,&owner,4)&&owner>=0x10000&&owner<=0xfffe9fa7u&&read(owner+0x16054,&index,4))out.converter=schemeConverter(read,0xB054D0,index);
 }else if(useCell){out.layer=Layer::Terrain;out.tintable=true;if(cellPointer>=0x10000&&cellPointer<=0xffffffc8u)read(cellPointer+0x34,&out.converter,4);
 }else if(read(object+0xD4,&out.converter,4)&&out.converter){out.ownIntensity=true;out.tintable=true;
 }else if(alt){out.tintable=true;out.converter=schemeConverter(read,0xB054D0,0);
 }else read(0x87F6C0,&out.converter,4); // ANIM ConvertClass has no LightConvert RGB fields.
 return out;
}
template<class Read>State animation(Read read,uint32_t object,uint32_t type,int x,int y,int z,const State&fallback){
 (void)z;uint32_t p=cellAt(read,x,y);auto source=animationSource(read,object,type,p);
 State out=cell(read,p,fallback,source.layer,false);
 if(source.ownIntensity){int intensity=-1;if(read(object+0xFC,&intensity,4)&&intensity>=0&&intensity<=2000)out.intensity=intensity;}
 if(source.tintable)converterTint(read,source.converter,out.tint);else out.tint={{1000,1000,1000}};converterMetadata(read,source.converter,out);
 uint8_t normal=0;if(type>=0x10000&&type<=0xfffffca2u&&read(type+0x35D,&normal,1)&&normal)out.intensity=1000;
 // A one-shade (UI/AltPalette) converter ignores the per-pixel brightness.
 int shades=0;if(source.converter>=0x10000&&source.converter<=0xfffffe90u&&read(source.converter+0x16C,&shades,4)&&shades==1)out.intensity=1000;
 return out;
}
// Linear reference transform used by pixel tests. The camera compositor uses
// its quantized Light transform at presentation time; neither transform bakes
// changing map tint into cached poses or changes their alpha.
inline uint32_t pixel(uint32_t p,const State&s){
 auto g=s.gains();unsigned a=p>>24;uint32_t out=a<<24;
 for(int n=0;n<3;++n){int shift=16-n*8;unsigned c=unsigned(std::clamp(float((p>>shift)&255)*g[n],0.f,float(a)));out|=c<<shift;}return out;
}
}}
