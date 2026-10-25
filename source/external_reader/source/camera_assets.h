#pragma once
#include "game_asset_files.h"
#include "asset_decode.h"
#include "visual_pose.h"
#include "art_metadata.h"
#include "cameo_catalog.h"
#include "shape_depth.h"
#include "building_animations.h"
#include "building_components.h"
#include "scene_lighting.h"
#include "sprite_names.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "../../unit_monitor/source/core.h"
namespace external { namespace cameraassets {
// One context per read session/thread; decoders and rules are shared by the
// observation windows and real-scene minimap without sharing mutable readers.
template<class Reader>class Context {
 Reader&reader_;uint32_t process_=0,match_=0;
public:
 struct Read {Reader*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->readFresh(p,out,n);}};
 Read read;
 explicit Context(Reader&r):reader_(r),read{&r},artINI(read,external::assets::LiveINI::ArtObject),rulesINI(read,external::assets::LiveINI::RulesPointer,true),cameoCatalog(read){}
 void identity(uint32_t process,uint32_t match){
  if(process_!=process||match_!=match){voxelLight={};voxelChecked=voxelNative=false;voxelRevision=~uint64_t(0);}
  process_=process;match_=match;update();
 }

external::assets::Files files;
std::string failure;
void update(){files.update(reader_,process_,match_);vpl();}
uint64_t revision(){return files.revision()^(uint64_t(word(0x822CF8)+1)*0x9e3779b97f4a7c15ull)^voxelLight.signature();}
bool cameo(const char*name,std::vector<uint8_t>&out){update();return files.load(name,out);}
uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
std::string text(uint32_t p,unsigned n){return external::assets::text(read,p,n);}
std::string theater(int field,int size){unsigned n=word(0x822CF8);return n<6?text(0x7E1B78+n*112+field,size):std::string{};}
external::assets::EffectiveINI<Read> artINI,rulesINI;uint64_t iniRevision=~uint64_t(0);
 std::shared_ptr<const external::voxel::Image> buildingMask;bool buildingMaskBiased=false;uint64_t maskRevision=~uint64_t(0);
 external::assets::RuntimeCameos<Read> cameoCatalog;external::voxel::Vpl voxelLight;uint64_t voxelRevision=~uint64_t(0);
 uint32_t voxelCheck=0;bool voxelChecked=false,voxelNative=false;
 void resolveCameo(um::Type&t){update();auto file=cameoCatalog.resolve([&](const char*n,std::vector<uint8_t>&b){return files.load(n,b);},files.revision(),files.revision()^(GetTickCount64()/5000),t.id,text(t.ptr+0x1F8,25),t.cameo);memset(t.cameo,0,sizeof t.cameo);if(file.size()<sizeof t.cameo)memcpy(t.cameo,file.data(),file.size());}
 void vpl(){
  auto revision=files.revision();if(voxelRevision!=revision){voxelRevision=revision;voxelLight={};voxelChecked=voxelNative=false;}
  uint32_t now=GetTickCount();if(voxelChecked&&uint32_t(now-voxelCheck)<1000)return;voxelCheck=now;voxelChecked=true;
  // Prefer the table actually used by the game, including modded archives
  // unavailable to the external file reader. Poll from update(), so cached
  // poses also recover after an initially missing table and change revision.
  if(voxelLight.capture(read)){voxelNative=true;return;}
  if(voxelLight.ready())return;
  std::vector<uint8_t>b;if(files.load("VOXELS.VPL",b))voxelLight.load(b);
 }
void metadata(){uint64_t revision=files.revision();uint64_t generation=revision^(GetTickCount64()/5000);artINI.generation(generation);rulesINI.generation(generation);if(revision==iniRevision)return;iniRevision=revision;artINI.clear();rulesINI.clear();std::vector<uint8_t>b;
 if(files.load("ARTMD.INI",b)||files.load("ART.INI",b))artINI.parse(b);
 if(files.load("RULESMD.INI",b)||files.load("RULES.INI",b))rulesINI.parse(b);
}
std::string artName(uint32_t type){metadata();std::string stem=text(type+0x1F8,25);if(stem.empty()){auto id=text(type+0x24,24);stem=rulesINI.get(id,"IMAGE");if(stem.empty())stem=id;}return external::assets::name(stem);}
std::string imageName(uint32_t type){auto stem=artName(type),image=artINI.get(stem,"IMAGE");return !image.empty()&&external::assets::safe(image)?external::assets::name(image):stem;}
std::string artValue(uint32_t type,const std::string&key){auto image=imageName(type),value=artINI.get(image,key);return value.empty()?artINI.get(artName(type),key):value;}
bool artYes(uint32_t type,const std::string&key){auto v=external::assets::name(artValue(type,key));return v=="YES"||v=="TRUE"||v=="1";}
bool paletteFile(const std::string&name,std::array<uint32_t,256>&out){std::vector<uint8_t>b;if(!files.load(name,b)){failure="palette-not-found:"+name;return false;}if(!external::assets::pal(b,out)){failure="palette-format:"+name;return false;}return true;}
bool convertPalette(uint32_t convert,std::array<uint32_t,256>&out){
 // Only LightConvert owns UsedPalette1 (+188). Plain ANIM ConvertClass ends
 // earlier; treating its following allocation as RGB would read unrelated data.
 if(convert<0x10000||convert>0xfffffe74u||word(convert)!=0x7ED0A4)return false;
 uint32_t pointer=word(convert+0x188);uint8_t colors[768]{};
 if(pointer<0x10000||uint64_t(pointer)+sizeof colors>0x100000000ull||!read(pointer,colors,sizeof colors)||
    pointer!=word(convert+0x188)||std::all_of(colors,colors+sizeof colors,[](uint8_t c){return c==0;}))return false;
 for(int i=0;i<256;++i)out[i]=0xff000000u|(uint32_t(colors[i*3])<<16)|(uint32_t(colors[i*3+1])<<8)|colors[i*3+2];out[0]=0;return true;
}
bool convertedPalette(uint32_t convert,std::array<uint32_t,256>&out){
 // Plain ConvertClass retains the rendered middle palette, not raw RGB.
 // 71C294 uses this resource drawer even when the map theater is temperate.
 if(convert<0x10000||convert>0xfffffe87u||word(convert)!=0x7E5358||word(convert+4)!=2)return false;
 int shades=int(word(convert+0x16C));uint32_t pointer=word(convert+0x174);uint16_t colors[256]{};
 if(shades<1||shades>256||pointer<0x10000||uint64_t(pointer)+sizeof colors>0x100000000ull||!read(pointer,colors,sizeof colors)||
    pointer!=word(convert+0x174)||word(convert)!=0x7E5358||word(convert+4)!=2||std::all_of(colors,colors+256,[](uint16_t c){return c==0;}))return false;
 for(int i=0;i<256;++i){unsigned c=colors[i],r=(c>>11)&31,g=(c>>5)&63,b=c&31;
  out[i]=0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);
 }out[0]=0;return true;
}
bool palette(bool iso,std::array<uint32_t,256>&out){if(iso&&external::terrain::palette(read,0xABBED0,out))return true;std::vector<uint8_t> bytes;std::string n=iso?theater(68,10):"UNIT"+theater(78,3);if(n.empty()){failure="palette-theater-name";return false;}n+=".PAL";if(!files.load(n,bytes)){failure="palette-not-found:"+n;return false;}if(!external::assets::pal(bytes,out)){failure="palette-format:"+n;return false;}return true;}
std::vector<std::string> spriteNames(uint32_t type,uint32_t pointer,bool overlay=false,bool buildingPart=false,uint32_t parentType=0){
 auto live=external::assets::shapeName(read,pointer);auto stem=imageName(type);
 bool seasonal=(word(type+0x22C)&255)||artYes(type,"THEATER");
 bool newTheater=(word(type+0x237)&255)||artYes(type,"NEWTHEATER");
 if(parentType>=0x10000)newTheater|=(word(parentType+0x237)&255)||artYes(parentType,"NEWTHEATER");
 return external::assets::spriteNames(stem,live,theater(78,3),theater(86,1),seasonal,newTheater,overlay,buildingPart);
}
bool shapeBytes(uint32_t type,uint32_t pointer,std::vector<uint8_t>&bytes,std::string&file,bool overlay=false,bool buildingPart=false,uint32_t parentType=0){
 auto names=spriteNames(type,pointer,overlay,buildingPart,parentType);for(const auto&n:names)if(files.load(n,bytes)){file=n;return true;}
 bytes.clear();
 file.clear();failure="sprite-not-found:"+artName(type);for(const auto&n:names)failure+=" "+n;return false;
}
bool spritePalette(uint32_t type,const std::string&file,bool overlay,uint32_t owner,std::array<uint32_t,256>&out,bool terrain=false){
 if(terrain&&external::lighting::spawnsTiberium(read,type)){
  uint32_t convert=word(0x87F6BC);if(convertPalette(convert,out)||convertedPalette(convert,out))return true;
  // Original bootstrap 52BFCE/52C1EA loads UNITSNO.PAL then aliases its
  // converter for SpawnsTiberium. This is separate from ore/gem overlays.
  return paletteFile("UNITSNO.PAL",out);
 }
 metadata();auto custom=artValue(type,"PALETTE");if(!overlay&&!terrain&&custom.empty())custom=text(type+0xDD0,32);
 auto id=text(type+0x24,24);bool resource=overlay&&(rulesINI.yes(id,"TIBERIUM")||rulesINI.yes(id,"ISVEINS")||rulesINI.yes(id,"ISVEINHOLEMONSTER"));
 bool seasonal=file.size()>4&&external::assets::name(file.substr(file.size()-4))!=".SHP";
 bool iso=artYes(type,"TERRAINPALETTE")||seasonal||terrain;
 // A type-specific live scheme already contains its custom palette and remap.
 // The global unit scheme cannot substitute for a missing custom scheme.
 if(!overlay&&!iso&&(custom.empty()||word(type+0xDF0))&&external::voxel::palette(read,type,owner,out))return true;
 bool ok=false;
 if(!custom.empty()){custom=external::assets::name(custom);if(custom.size()>4&&custom.substr(custom.size()-4)==".PAL")ok=paletteFile(custom,out);
  else ok=paletteFile(custom+theater(78,3)+".PAL",out)||paletteFile(custom+".PAL",out);
 }else if(resource)ok=paletteFile("TEMPERAT.PAL",out); // FA2's RA2 ore/gems palette
 else if(iso&&seasonal)ok=paletteFile("ISO"+external::assets::name(file.substr(file.size()-3))+".PAL",out);
 else ok=palette(iso,out);
 if(!ok)return false;
 if(!iso&&!overlay&&owner>=0x10000){uint8_t rgb[3]{};if(read(owner+22265,rgb,3))for(int i=16;i<32;++i){unsigned f=unsigned(8+i-16);out[i]=0xff000000u|((unsigned(rgb[0])*f/23)<<16)|((unsigned(rgb[1])*f/23)<<8)|(unsigned(rgb[2])*f/23);}}
 return true;
}
double conditionYellow(){
 double live=0;uint32_t rules=word(0x8871E0);if(rules>=0x10000&&read(rules+0x1700,&live,sizeof live)&&std::isfinite(live)&&live>=0&&live<=1)return live;
 metadata();auto value=rulesINI.get("AUDIOVISUAL","CONDITIONYELLOW");char*end=nullptr;double threshold=strtod(value.c_str(),&end);if(end&&*end=='%')threshold/=100.;
 return value.empty()||!std::isfinite(threshold)||threshold<0||threshold>1?.5:threshold;
}
int buildingFrame(uint32_t object,uint32_t type){
 int frame=int32_t(word(object+0xF8)),state=int32_t(word(object+0x534));
 if(word(type+0x16BF)&255)return int32_t(word(object+0x618));
 if(word(type+0x16C0)&255)return int32_t(word(object+0x61C));
 if(state==0){int start=int32_t(word(type+0xF04)),count=int32_t(word(type+0xF08));
  if(word(type+0x16B7)&255)frame=start+count-frame-1;
  int mission=int32_t(word(object+0xAC));if(mission==-1)mission=int32_t(word(object+0xB4));
  if(mission==19)frame=start+count-frame-1;
  return frame>=0&&frame<8192?frame:-1;
 }
 int strength=int32_t(word(type+0xA0)),health=int32_t(word(object+0x6C));if(strength<=0||health<=0)return 0;
 return std::clamp(frame,0,8191)+(double(health)/strength<=conditionYellow()?1:0);
}
void buildingPose(uint32_t object,uint32_t type,external::visual::Pose&pose){
 pose.frame=buildingFrame(object,type);auto state=external::buildings::state(read,object,type,conditionYellow());
 pose.buildingFlags=state.flags;pose.doorFrame=state.doorFrame;
 external::GameView game(read);if(external::buildings::buildup(game,object))pose.variant=-2;else if(pose.variant==-2)pose.variant=-1;
}
bool overlay(uint32_t type,uint32_t pointer,int frame,external::voxel::Image&out,bool terrain=false){std::vector<uint8_t>bytes;std::string file;bool disk=shapeBytes(type,pointer,bytes,file,!terrain);std::array<uint32_t,256>pal{};if(!spritePalette(type,file,true,0,pal,terrain))return false;
 std::string why;bool ok=disk?external::voxel::sprite(external::assets::Memory{std::move(bytes)},external::assets::Memory::Base,pal,out,frame,&why):external::voxel::sprite(read,pointer,pal,out,frame,&why);if(!ok)failure="overlay-frame/decode:"+file+" frame="+std::to_string(frame)+" "+why;
 if(ok&&terrain&&external::lighting::spawnsTiberium(read,type))out.y-=16; // Native 71C2A9 / 71C438.
 if(ok&&!terrain){out.flat=(word(type+0x2B3)&255)!=0;bool wall=(word(type+0x2A8)&255)!=0,rock=(word(type+0x2B5)&255)!=0;out.depthBias=out.flat?-1.f:(wall||rock)?0.f:15.f;out.writeDepth=(word(type+0x2AD)&255)==0;}
 return ok;}
bool sprite(uint32_t type,uint32_t pointer,const std::array<uint32_t,256>&pal,int frame,external::voxel::Image&out){
 std::vector<uint8_t> bytes;std::string filename;if(!shapeBytes(type,pointer,bytes,filename))return false;
 if(external::assets::shp(std::move(bytes),frame,pal,out))return true;failure="sprite-load/decode:"+filename+" frame="+std::to_string(frame);return false;
}
bool tile(uint32_t type,unsigned sub,const std::array<uint32_t,256>&pal,external::voxel::Image&out){
 std::vector<uint8_t> bytes;auto filename=text(type+0x2F5,14);std::string reason;
 if(!filename.empty()&&files.load(filename,bytes)&&external::assets::tmp(std::move(bytes),sub,pal,out,&reason))return true;
 if(external::terrain::tile(read,word(type+0xA4),sub,pal,out,&reason)){failure.clear();return true;}
 failure="tile-not-found:"+filename+" memory="+reason;return false;
}
bool actor(uint32_t p,uint32_t type,uint32_t id,double angle,external::voxel::Image&out){
 if(word(p+0x10)!=id)return false;uint32_t owner=word(p+0x21C);std::array<uint32_t,256> pal{};

 auto stem=text(type+0x1F8,25);if(stem.empty())stem=text(type+0x24,24);std::vector<uint8_t> v,h;external::voxel::Model model;
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 if(!stem.empty()&&files.load(stem+".VXL",v)&&files.load(stem+".HVA",h)&&external::assets::vxl(v,h,model,0,float(sx),float(sy))){
  int offset=int32_t(word(type+0x720));if(offset<-4096||offset>4096)offset=0;
  for(auto part:{"TUR","BARL"})if(files.load(stem+part+".VXL",v)&&files.load(stem+part+".HVA",h))external::assets::vxl(v,h,model,float(offset)*12/256,float(sx),float(sy));
  return spritePalette(type,"",false,owner,pal)&&external::voxel::render(model,angle,pal,out)&&word(p+0x10)==id;
 }external::GameView game(read);int heading=int(std::lround(angle*32/(2*external::visual::Pi)))&31;
 std::vector<uint8_t> shape;std::string file;if(!shapeBytes(type,word(type+0xA4),shape,file)||!spritePalette(type,file,false,owner,pal))return false;
 return external::assets::shp(std::move(shape),external::visual::previewFrame(game,type,game.kind(p),heading),pal,out)&&word(p+0x10)==id;
}
// Observation assets are retained separately from rendered headings: turning a
// unit must not reopen/redecode its VXL/HVA files every overlay frame.
struct BuildingPart {uint32_t pointer=0;std::vector<uint8_t> shape;std::string filename;};
struct ActorArt {
 external::voxel::Model model;std::vector<uint8_t> shape;std::array<uint32_t,256> colors{};
 std::string filename;
 uint32_t pointer=0,type=0;float turretOffset=0;bool voxel=false;int kind=-1,fw=1,fh=1,moveX=0,moveY=0,drawY=0,zAdjust=0;bool flat=false,shadow=true;
 std::shared_ptr<const external::voxel::Image> depthMask;bool depthMaskBiased=false;
 std::array<BuildingPart,external::buildings::PartCount> buildingParts;
};
static size_t cost(const ActorArt&a){size_t n=a.shape.size()+a.model.faces.size()*sizeof(external::voxel::Face);for(const auto&part:a.buildingParts)n+=part.shape.size();return n;}
static int integer(const std::string&s,int fallback=0){if(s.empty())return fallback;char*end=nullptr;long n=strtol(s.c_str(),&end,10);return end!=s.c_str()&&!*end?int(std::clamp(n,-8192l,8192l)):fallback;}
void captureBuildingParts(uint32_t type,ActorArt&art){
 for(int i=0;i<external::buildings::PartCount;++i){auto&part=art.buildingParts[i];part.pointer=word(type+external::buildings::partOffsets[i]);if(part.pointer<0x10000){part.pointer=0;continue;}
  auto live=external::assets::shapeName(read,part.pointer);auto stem=i==external::buildings::Bib?artValue(type,"BIBSHAPE"):std::string{};
  if(stem.size()>4&&external::assets::name(stem.substr(stem.size()-4))==".SHP")stem.resize(stem.size()-4);
  // A body Image name is not a fallback for a factory door. Loaded parts can
  // decode directly from memory; unloaded references retain their real name.
  auto names=external::assets::spriteNames(stem,live,theater(78,3),theater(86,1),(word(type+0x22C)&255)!=0,(word(type+0x237)&255)!=0,false,true);
  part.filename=live;for(const auto&name:names)if(files.load(name,part.shape)){part.filename=name;break;}
 }
}
bool captureAnimation(uint32_t p,uint32_t type,uint32_t id,uint32_t parent,uint32_t parentId,ActorArt&out){
 external::animations::State state;
 if(!external::animations::capture(read,p,parent,parentId,state)||state.id!=id||state.type!=type){failure="animation-identity/slot";return false;}
 ActorArt next;next.kind=5;next.type=type;next.pointer=word(type+0xA4);next.flat=(word(type+0x369)&255)!=0;next.shadow=(word(type+0x372)&255)!=0;
 next.drawY=int32_t(word(type+0x344));if(std::abs(int64_t(next.drawY))>4096)return false;
 bool loaded=shapeBytes(type,next.pointer,next.shape,next.filename,false,true,word(parent+0x520));
 if(!loaded)next.filename=external::assets::shapeName(read,next.pointer);
 auto source=external::lighting::animationSource(read,p,type,external::lighting::cellAt(read,state.x,state.y));
 bool colors=source.tintable&&convertPalette(source.converter,next.colors);
 auto custom=text(p+0xDC,32);if(custom.empty())custom=artValue(type,"PALETTE");
 if(!colors&&!custom.empty()){custom=external::assets::name(custom);colors=custom.size()>4&&custom.substr(custom.size()-4)==".PAL"?paletteFile(custom,next.colors):(paletteFile(custom+theater(78,3)+".PAL",next.colors)||paletteFile(custom+".PAL",next.colors));}
 else if(!colors&&parent>=0x10000&&(word(p+0x118)&255))colors=spritePalette(word(parent+0x520),"",false,state.owner,next.colors);
 else if(!colors)colors=paletteFile("ANIM.PAL",next.colors);
 if(!colors)return false;
 // A theater SHP already loaded by the engine remains usable when its archive
 // cannot be resolved on disk. Frame zero is valid.
 if(!loaded){external::voxel::Image check;std::string why;
  if(!external::voxel::sprite(read,next.pointer,next.colors,check,state.frame,&why,255)){failure="animation-memory:"+why;return false;}}
 external::animations::State after;
 if(!external::animations::capture(read,p,parent,parentId,after)||after.id!=id||after.type!=type||after.owner!=state.owner){failure="animation-changed";return false;}
 failure.clear();out=std::move(next);return true;
}
bool captureActor(uint32_t p,uint32_t type,uint32_t id,ActorArt&out){
 failure.clear();if(word(p+0x10)!=id){failure="actor-identity";return false;}ActorArt next;external::GameView game(read);next.kind=game.kind(p);next.type=type;uint32_t owner=word(p+0x21C);
 metadata();next.shadow=!rulesINI.yes(text(type+0x24,24),"NOSHADOW");next.flat=artYes(type,"FLAT");
 if(next.kind==3){
  auto foundation=external::assets::name(artValue(type,"FOUNDATION"));int fw=1,fh=1;
  if(foundation=="CUSTOM"){fw=integer(artValue(type,"FOUNDATION.X"),1);fh=integer(artValue(type,"FOUNDATION.Y"),1);}else if(sscanf(foundation.c_str(),"%dX%d",&fw,&fh)!=2)fw=fh=1;
  external::animations::foundation(read,type,fw,fh);next.fw=std::clamp(fw,1,32);next.fh=std::clamp(fh,1,32);auto move=artValue(type,"ZSHAPEPOINTMOVE");sscanf(move.c_str(),"%d,%d",&next.moveX,&next.moveY);
  read(type+0x1530,&next.moveX,4);read(type+0x1534,&next.moveY,4);
  next.moveX=std::clamp(next.moveX,-4096,4096);next.moveY=std::clamp(next.moveY,-4096,4096);next.zAdjust=integer(artValue(type,"NORMALZADJUST"));
  int liveAdjust=0;if(read(type+0x1520,&liveAdjust,4)&&std::abs(int64_t(liveAdjust))<=8192)next.zAdjust=liveAdjust;
  if(maskRevision!=files.revision()){maskRevision=files.revision();buildingMask.reset();std::vector<uint8_t>b;std::array<uint32_t,256>dummy{};dummy.fill(0xffffffff);
   auto mask=std::make_shared<external::voxel::Image>();
   buildingMaskBiased=external::voxel::sprite(read,word(0x89DDBC),dummy,*mask,0,nullptr,255);
   if(buildingMaskBiased||((files.load("BUILDNGZ.SHA",b)||files.load("BUILDNGZ.SHP",b))&&external::assets::shp(std::move(b),0,dummy,*mask)))buildingMask=std::move(mask);}
  if(next.fw<8){next.depthMask=buildingMask;next.depthMaskBiased=buildingMaskBiased;} // native 43D787 skips the cone for width >= 8
 }

 auto stem=text(type+0x1F8,25);if(stem.empty())stem=text(type+0x24,24);
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 auto part=[&](const std::string&suffix,uint32_t at,bool turret){
  external::voxel::Model model;std::vector<uint8_t> v,h;
  bool ok=!stem.empty()&&files.load(stem+suffix+".VXL",v)&&files.load(stem+suffix+".HVA",h)&&external::assets::vxl(v,h,model,0,float(sx),float(sy));
  if(!ok)ok=external::voxel::capturePart(read,word(at),word(at+4),model,0,float(sx),float(sy));
  if(ok){for(auto&f:model.faces)f.turret=turret;next.model.faces.insert(next.model.faces.end(),model.faces.begin(),model.faces.end());}return ok;
 };
 if(part("",type+0xB0,false)){
  if(!spritePalette(type,"",false,owner,next.colors))return false;
  next.voxel=true;int offset=int32_t(word(type+0x720));if(offset<-4096||offset>4096)return false;next.turretOffset=float(offset)*12/256*float(sx);
  int variant=-1;if(int(word(type+0x808))>0&&!uint8_t(word(type+0xCD5)))variant=int(word(p+0x124));
  if(variant>=0&&variant<18){
   // Native 73B90E and 70DCF0 select ExtraBarrels/ExtraTurrets by
   // CurrentTurretNumber (+124), not CurrentWeaponNumber (+138).
   for(uint32_t at:{type+0xC8+uint32_t(variant)*8,type+0x158+uint32_t(variant)*8}){
    external::voxel::Model component;if(external::voxel::capturePart(read,word(at),word(at+4),component,0,float(sx),float(sy))){for(auto&f:component.faces)f.turret=true;next.model.faces.insert(next.model.faces.end(),component.faces.begin(),component.faces.end());}
   }
  }else{part("TUR",type+0xB8,true);part("BARL",type+0xC0,true);}
 }else{
  bool buildup=next.kind==3&&external::buildings::buildup(game,p);
  next.pointer=word(type+(buildup?0xE00:0xA4));std::string filename;bool loaded=false;
  if(buildup){auto stem=text(type+0xE5C,16);if(stem.empty())stem=artValue(type,"Buildup");
   auto names=external::assets::spriteNames(stem,external::assets::shapeName(read,next.pointer),theater(78,3),theater(86,1),(word(type+0x22C)&255)!=0,(word(type+0x237)&255)!=0,false,true);
   for(const auto&name:names)if(files.load(name,next.shape)){filename=name;loaded=true;break;}
  }else loaded=shapeBytes(type,next.pointer,next.shape,filename,false,next.kind==3);next.filename=filename;
  if(!loaded)filename=external::assets::shapeName(read,next.pointer);
  bool terrainPalette=next.kind==3&&(word(type+0x1702)&255);
  if(terrainPalette?!palette(true,next.colors):!spritePalette(type,filename,false,owner,next.colors))return false;
  if(!loaded){external::voxel::Image check;if(!external::voxel::sprite(read,next.pointer,next.colors,check)){failure="sprite-not-found/unloaded:"+(filename.empty()?stem:filename);return false;}}
 }
 if(next.kind==3)captureBuildingParts(type,next);
 if(word(p+0x10)!=id||word(p+0x21C)!=owner)return false;out=std::move(next);return true;
}
bool renderActor(const ActorArt&art,const external::visual::Pose&pose,external::voxel::Image&out){
 auto angle=external::visual::radians(pose.body),turret=external::visual::radians(pose.turret);
 if(art.voxel){vpl();if(!external::voxel::render(art.model,angle,art.colors,out,turret,art.turretOffset,&voxelLight,255))return false;
  // The model Z buffer resolves faces inside the cached voxel only. The game
  // submits that cached image through the same standing shape Z test as SHP.
  // Orthographic model depth is not in the terrain/BUILDNGZ buffer's units.
  out.depth.clear();out.writeDepth=false;
  if(art.shadow){auto ground=art.model;for(auto&face:ground.faces)for(auto&p:face.p)p.z=0;external::voxel::Image shadow;
   if(external::voxel::render(ground,angle,art.colors,shadow,turret,art.turretOffset,nullptr,255)){shadow.x+=3;shadow.shadow=true;shadow.flat=true;shadow.writeDepth=false;shadow.depthBias=2;shadow.depth.clear();out.attachments.push_back(std::move(shadow));}}
  return true;
 }
 if(pose.frame<0){failure="sprite-sequence";return false;}
 auto decode=[&](const std::vector<uint8_t>&shape,uint32_t pointer,int frame,external::voxel::Image&image,std::string&why){
  auto bytes=[&](uint32_t at,void*p,size_t n){if(at<external::assets::Memory::Base||uint64_t(at-external::assets::Memory::Base)+n>shape.size())return false;memcpy(p,shape.data()+at-external::assets::Memory::Base,n);return true;};
  return !shape.empty()?external::voxel::sprite(bytes,external::assets::Memory::Base,art.colors,image,frame,&why,255):external::voxel::sprite(read,pointer,art.colors,image,frame,&why,255);
 };
 auto frames=[&](const std::vector<uint8_t>&shape,uint32_t pointer){uint8_t head[8]{};if(!shape.empty()){if(shape.size()<8)return 0;memcpy(head,shape.data(),8);}
  else{if(pointer<0x10000||!read(pointer,head,8))return 0;if(external::voxel::u16(head)==65535){pointer=word(pointer+12);if(pointer<0x10000||!read(pointer,head,8))return 0;}}
  return external::voxel::u16(head)==0?int(external::voxel::u16(head+6)):0;};
 const auto*shape=&art.shape;uint32_t pointer=art.pointer;std::string filename=art.filename;int frame=pose.frame;float bias=-float(art.zAdjust);
 bool unloading=art.kind==3&&(pose.buildingFlags&external::buildings::Unloading),roof=(pose.buildingFlags&external::buildings::Roof)!=0;
 if(unloading){const auto&part=art.buildingParts[roof?external::buildings::RoofDeploy:external::buildings::Deploy];if(part.pointer){shape=&part.shape;pointer=part.pointer;filename=part.filename;bias=roof?40.f:20.f;}}
 // 43D7A7 clamps every main building shape, including make art, to half the
 // selected SHP's frame count. Bib receives the original GetCurrentFrame.
 if(art.kind==3){int n=frames(*shape,pointer);if(n>0)frame=std::min(frame,n/2);}
 bool complete=true;std::string why;if(!decode(*shape,pointer,frame,out,why)){
  failure="sprite-frame/decode:"+filename+" frame="+std::to_string(frame)+" "+why;
  if(shape==&art.shape)return false;
  // A demand-loaded production SHP may be unavailable while the body is
  // already readable. Retain that body and retry the incomplete composition.
  complete=false;shape=&art.shape;pointer=art.pointer;frame=pose.frame;bias=-float(art.zAdjust);int n=frames(*shape,pointer);if(n>0)frame=std::min(frame,n/2);
  if(!decode(*shape,pointer,frame,out,why))return false;
 }
 out.y+=art.drawY;out.objectPalette=true;out.flat=art.flat;out.writeDepth=art.kind==3;out.depthBias=bias;
 if(art.kind==3&&!out.flat&&art.depthMask)external::camera::buildingDepth(out,*art.depthMask,art.fw,art.fh,art.moveX,art.moveY,art.depthMaskBiased);
 auto shadow=[&](const std::vector<uint8_t>&bytes,uint32_t at,int displayed,external::voxel::Image&image,bool write,float shadowBias){int n=frames(bytes,at);if(!art.shadow||n<2||displayed<0||displayed>=n/2)return;external::voxel::Image part;
  if(decode(bytes,at,displayed+n/2,part,why)){part.y+=art.drawY;part.shadow=true;part.flat=true;part.writeDepth=write;image.shadowsAfter=art.kind==3||write;part.depthBias=shadowBias;image.attachments.push_back(std::move(part));}};
 // DrawObject's shadow branch recomputes its ground plane independently of
 // NormalZAdjust and the -20/-40 production-body adjustment (706469/70653E).
 shadow(*shape,pointer,frame,out,art.kind==3,art.kind==3?2.f:out.depthBias+(art.kind==5?0:2));
 auto component=[&](external::buildings::Part index,int displayed,float depthBias,bool write){const auto&part=art.buildingParts[index];if(!part.pointer||displayed<0)return;
  external::voxel::Image image;if(!decode(part.shape,part.pointer,displayed,image,why)){failure="building-part/decode:"+part.filename+" frame="+std::to_string(displayed)+" "+why;complete=false;return;}
  image.y+=art.drawY;image.objectPalette=true;image.flat=true;image.writeDepth=write;image.depthBias=depthBias;
  shadow(part.shape,part.pointer,displayed,image,write,2);out.attachments.push_back(std::move(image));
 };
 if(art.kind==3){
  // Native order: body (43D85F), Bib (43D8E9), UnderDoor/UnderRoof
  // (43D9C9/43DA67), Door (43DF3F). All three extras use the ground gradient;
  // only the standing body uses BUILDNGZ. Their own state supplies the frame.
  if(pose.buildingFlags&external::buildings::Constructed)component(external::buildings::Bib,pose.frame,1,true);
  if(unloading){component(roof?external::buildings::UnderRoofDoor:external::buildings::UnderDoor,(pose.buildingFlags&external::buildings::Damaged)?1:0,0,true);component(external::buildings::Door,pose.doorFrame,5,false);}
 }
 out.incomplete=!complete;return true;
}


};
}}
