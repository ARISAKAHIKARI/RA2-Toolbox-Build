#pragma once
#include "terrain_sprite.h"
#include "game_asset_files.h"
#include "asset_decode.h"
#include "radar_shroud_assets.h"
#include "visual_pose.h"
#include "art_metadata.h"
#include "cameo_catalog.h"
#include "shape_depth.h"
#include "building_animations.h"
#include "building_components.h"
#include "scene_lighting.h"
#include "sprite_names.h"
#include "projectile_visuals.h"
#include "particle_visuals.h"
#include "voxel_animation_visuals.h"
#include "live_palette_cache.h"
#include "first_field_reads.h"
#include "palette_recovery.h"
#include "gpu_voxel.h"
#include "gpu_voxel_cache.h"
#include "voxel_geometry_cache.h"
#include "native_image_bank.h"
#include "camera_raster.h"
#include <map>
#include <tuple>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include "../../unit_monitor/source/core.h"
namespace external { namespace cameraassets {
// One context per read session/thread; decoders and rules are shared by the
// observation windows and real-scene minimap without sharing mutable readers.
template<class Reader>class Context {
 Reader&reader_;uint32_t process_=0,match_=0;
 uint64_t presentationRevision_=0;uint32_t presentationTheater_=0;bool presentationRevisionKnown_=false;
 bool lastPaletteLive_=false,terrainPaletteLive_=false,presentation_=false,presentationVplFresh_=false;
 bool colorPrecision_=false,gpuPresentation_=false,voxelRelight_=true,spriteRelight_=true;
 gpuvoxel::PacketCache gpuPackets_;
 voxelgeometry::Cache voxelGeometry_;
 std::map<std::array<uint32_t,256>,std::weak_ptr<const std::vector<uint32_t>>>gpuPalettes_;
 std::shared_ptr<const gpuvoxel::PacketCache::VplSnapshot>gpuVplSnapshot_;
 uint64_t gpuVplSignature_=0;
 int detail_=0;
 resolutionsampling::Selection resolution_;
 pixelupgrade::PaletteRange paletteRange_=pixelupgrade::PaletteRange::OriginalDAC;
 struct PaletteSample {std::array<uint32_t,256>colors{};bool live=false;};
 using PaletteSampleKey=std::tuple<uint32_t,uint32_t,std::string,bool,bool>;
 std::map<PaletteSampleKey,PaletteSample>presentationPalettes_;
 uint64_t paletteSamples_=0,paletteSampleHits_=0;
 using SharedTileKey=std::tuple<uint32_t,uint32_t,uint32_t,uint32_t,unsigned,uint64_t,std::array<uint32_t,256>>;
 inline static nativeimages::BasicBank<SharedTileKey> sharedTiles_{rendermemory::SharedTiles};
public:
 struct Read {Reader*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->readFresh(p,out,n);}
  template<class R>static auto nullable(R&r,uint32_t p,void*out,size_t n,int)->decltype(r.readOptional(p,out,n)){return r.readOptional(p,out,n);}
  template<class R>static bool nullable(R&r,uint32_t p,void*out,size_t n,long){return r.readFresh(p,out,n);}
  bool optional(uint32_t p,void*out,size_t n)const{return nullable(*reader,p,out,n,0);}
 };
 // Native resource bytes have their own resource/pointer revision and decoded
 // caches. They are not dynamic world facts and must never become per-frame
 // Runtime demands. Owner colors, object identities, poses, lights and live
 // converter palettes continue through Read and the pinned world payload.
 struct ResourceRead {
  Reader*reader;
  template<class R>static auto bytes(R&r,uint32_t p,void*out,size_t n,int)->decltype(r.readLive(p,out,n)){return r.readLive(p,out,n);}
  template<class R>static bool bytes(R&r,uint32_t p,void*out,size_t n,long){return r.readFresh(p,out,n);}
  bool operator()(uint32_t p,void*out,size_t n)const{return bytes(*reader,p,out,n,0);}
  bool readFresh(uint32_t p,void*out,size_t n)const{return operator()(p,out,n);}
  bool imagePath(wchar_t*out,DWORD*n)const{return reader->imagePath(out,n);}
 };
 Read read;
 ResourceRead resourceRead;
 void presentationColorPrecision(bool enabled){colorPrecision_=enabled;}
 void presentationDetail(int detail){detail_=std::clamp(detail,0,1);}
 void presentationGPU(bool enabled){gpuPresentation_=enabled;}
 bool presentationGPU()const{return gpuPresentation_;}
 void presentationLighting(bool voxel,bool sprite){voxelRelight_=voxel;spriteRelight_=sprite;}
 bool voxelRelighting()const{return voxelRelight_;}
 bool spriteRelighting()const{return spriteRelight_;}
 uint64_t paletteSamples()const{return paletteSamples_;}
 uint64_t paletteSampleHits()const{return paletteSampleHits_;}
 uint64_t gpuPreparedBuilds()const{return gpuPackets_.builds();}
 uint64_t gpuPreparedHits()const{return gpuPackets_.hits();}
 size_t gpuPreparedCacheBytes()const{return gpuPackets_.bytes();}
 uint64_t voxelGeometryBuilds()const{return voxelGeometry_.builds();}
 uint64_t voxelGeometryHits()const{return voxelGeometry_.hits();}
 uint64_t voxelGeometryFaceChecks()const{return voxelGeometry_.faceChecks();}
 uint64_t voxelGeometryEvictions()const{return voxelGeometry_.evictions();}
 size_t voxelGeometryCacheBytes()const{return voxelGeometry_.bytes();}
 bool presentationResolution(int sourceWidth,int sourceHeight,int targetWidth,int targetHeight){resolution_=resolutionsampling::choose(sourceWidth,sourceHeight,targetWidth,targetHeight);return resolution_.valid;}
 int presentationRasterScale()const{return detail_?(resolution_.valid?resolution_.rasterScale:0):1;}
 void presentationPalette(pixelupgrade::PaletteRange mode){paletteRange_=mode;}
 explicit Context(Reader&r):reader_(r),read{&r},resourceRead{&r},artINI(resourceRead,external::assets::LiveINI::ArtObject),rulesINI(resourceRead,external::assets::LiveINI::RulesPointer,true),cameoCatalog(resourceRead){}
 void identity(uint32_t process,uint32_t match){
  if(process_!=process||match_!=match){presentationRevision_=0;presentationRevisionKnown_=false;presentationPalettes_.clear();overlayPalettes_.clear();gpuPackets_.clear();voxelGeometry_.clear();gpuPalettes_.clear();gpuVplSnapshot_.reset();gpuVplSignature_=0;terrainPaletteLive_=presentation_=presentationVplFresh_=false;voxelLight={};voxelChecked=voxelNative=false;voxelRevision=~uint64_t(0);}
  process_=process;match_=match;update();
 }

external::assets::Files files;
std::string failure;
void update(){files.update(resourceRead,process_,match_);vpl();}
// An unavailable theatre is not theatre zero. A render may only replace its
// complete front after this checked snapshot; legacy cache users retain the
// last key on a transient read failure. identity() retires it across worlds.
bool revisionChecked(uint64_t&out){uint32_t theater=0;if(!read(0x822CF8,&theater,sizeof theater)||theater>=6)return false;
 out=files.revision()^(uint64_t(theater+1)*0x9e3779b97f4a7c15ull)^
    (paletteRange_==pixelupgrade::PaletteRange::FullRGB?0x45e391acbbdb1891ull:0)^(detail_?0x312df964ba273d19ull:0)^(detail_?uint64_t(presentationRasterScale())*0xf7e71b69894eb76bull:0)^(colorPrecision_?0x5a730c99b194d6e3ull:0);
 presentationRevision_=out;presentationTheater_=theater;presentationRevisionKnown_=true;return true;}
uint64_t revision(){uint64_t value=0;return revisionChecked(value)?value:presentationRevision_;}
uint64_t voxelLightingRevision()const{return voxelLight.signature();}
bool voxelLightingAvailable()const{return !voxelRelight_||!presentation_||presentationVplFresh_;}
bool cameo(const char*name,std::vector<uint8_t>&out){update();return files.load(name,out);}
uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
std::string text(uint32_t p,unsigned n){return external::assets::text(read,p,n);}
std::string theater(int field,int size){uint32_t n=presentationTheater_;if(!presentationRevisionKnown_&&(!read(0x822CF8,&n,sizeof n)||n>=6))return {};return text(0x7E1B78+n*112+field,size);}
external::assets::EffectiveINI<ResourceRead> artINI,rulesINI;uint64_t iniRevision=~uint64_t(0);
 std::shared_ptr<const external::voxel::Image> buildingMask;bool buildingMaskBiased=false;uint64_t maskRevision=~uint64_t(0);
 external::assets::RuntimeCameos<ResourceRead> cameoCatalog;external::voxel::Vpl voxelLight;uint64_t voxelRevision=~uint64_t(0);
 uint32_t voxelCheck=0;bool voxelChecked=false,voxelNative=false;
 void gpuVplSnapshot(bool force=false){
  if(!voxelLight.ready()){gpuVplSnapshot_.reset();gpuVplSignature_=0;return;}
  if(!force&&gpuVplSnapshot_&&gpuVplSignature_==voxelLight.signature())return;
  gpuvoxel::PacketCache::VplSnapshot next{};
  for(unsigned page=0;page<256;++page)for(unsigned index=0;index<256;++index)next[page*256+index]=voxelLight.map(page,uint8_t(index));
  if(!gpuVplSnapshot_||*gpuVplSnapshot_!=next)gpuVplSnapshot_=std::make_shared<const gpuvoxel::PacketCache::VplSnapshot>(std::move(next));
  gpuVplSignature_=voxelLight.signature();
 }
 void resolveCameo(um::Type&t){update();auto file=cameoCatalog.resolve([&](const char*n,std::vector<uint8_t>&b){return files.load(n,b);},files.revision(),files.revision()^(GetTickCount64()/5000),t.id,text(t.ptr+0x1F8,25),t.cameo);memset(t.cameo,0,sizeof t.cameo);if(file.size()<sizeof t.cameo)memcpy(t.cameo,file.data(),file.size());}
 void vpl(bool force=false){
  auto revision=files.revision();if(voxelRevision!=revision){voxelRevision=revision;voxelLight={};voxelChecked=voxelNative=false;}
  if(!voxelRelight_&&voxelLight.ready())return; // freeze the verified table; no live table recapture
  uint32_t now=GetTickCount();if(!force&&voxelChecked&&uint32_t(now-voxelCheck)<1000)return;voxelCheck=now;voxelChecked=true;
  if(force)presentationVplFresh_=false;
  // Prefer the table actually used by the game, including modded archives
  // unavailable to the external file reader. Legacy users poll from update();
  // live presentation captures every pass and keeps geometry revision separate.
  // The live double-read above is still mandatory. An unchanged, freshly
  // verified VPL need not expand the same 32 pages into 256 GPU pages again.
  if(voxelLight.capture(read)){voxelNative=true;if(force)presentationVplFresh_=true;if(gpuPresentation_)gpuVplSnapshot();return;}
  if(force)return; // current-presentation VXL never uses stale or guessed light tables
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
bool paletteFile(const std::string&name,std::array<uint32_t,256>&out){lastPaletteLive_=false;std::vector<uint8_t>b;if(!files.load(name,b)){failure="palette-not-found:"+name;return false;}if(!external::assets::pal(b,out,paletteRange_)){failure="palette-format:"+name;return false;}return true;}
void recoverPalettePrecision(const char*source,std::array<uint32_t,256>&live){
 if(!colorPrecision_||paletteRange_!=pixelupgrade::PaletteRange::FullRGB)return;
 // Reading the optional original PAL must not turn this authoritative live
 // drawer into a file fallback or replace its useful diagnostics on failure.
 const bool provenance=lastPaletteLive_;const std::string previousFailure=failure;
 std::array<uint32_t,256> candidate{};
 if(paletteFile(source,candidate))external::paletterecovery::promote(live,candidate);
 lastPaletteLive_=provenance;failure=previousFailure;
}
bool convertPalette(uint32_t convert,std::array<uint32_t,256>&out){
 // Only LightConvert owns UsedPalette1 (+188). Plain ANIM ConvertClass ends
 // earlier; treating its following allocation as RGB would read unrelated data.
 if(convert<0x10000||convert>0xfffffe74u||word(convert)!=0x7ED0A4)return false;
 uint32_t pointer=word(convert+0x188);uint8_t colors[768]{};
 if(pointer<0x10000||uint64_t(pointer)+sizeof colors>0x100000000ull||!read(pointer,colors,sizeof colors)||
    pointer!=word(convert+0x188)||std::all_of(colors,colors+sizeof colors,[](uint8_t c){return c==0;}))return false;
 for(int i=0;i<256;++i)out[i]=0xff000000u|(uint32_t(colors[i*3])<<16)|(uint32_t(colors[i*3+1])<<8)|colors[i*3+2];out[0]=0;lastPaletteLive_=true;return true;
}
bool convertedPalette(uint32_t convert,std::array<uint32_t,256>&out){
 // Plain ConvertClass retains the rendered middle palette, not raw RGB.
 // 71C294 uses this resource drawer even when the map theater is temperate.
 if(convert<0x10000||convert>0xfffffe87u||word(convert)!=0x7E5358||word(convert+4)!=2)return false;
 int shades=int(word(convert+0x16C));uint32_t pointer=word(convert+0x174);uint16_t colors[256]{};
 if(shades<1||shades>256||pointer<0x10000||uint64_t(pointer)+sizeof colors>0x100000000ull||!read(pointer,colors,sizeof colors)||
    pointer!=word(convert+0x174)||word(convert)!=0x7E5358||word(convert+4)!=2||std::all_of(colors,colors+256,[](uint16_t c){return c==0;}))return false;
 std::array<int,6>format{};bool rgb555=optionalread::bytes(read,0x8A0DD0,format.data(),sizeof format)&&format==std::array<int,6>{{10,3,0,3,5,3}};
 for(int i=0;i<256;++i){out[i]=nativepacked::color(colors[i],0xff000000u,rgb555);
 }out[0]=0;lastPaletteLive_=true;return true;
}
bool palette(bool iso,std::array<uint32_t,256>&out){lastPaletteLive_=false;if(iso&&external::terrain::palette(read,0xABBED0,out)){lastPaletteLive_=true;terrainPaletteLive_=true;return true;}if(iso&&terrainPaletteLive_)return false;std::vector<uint8_t> bytes;std::string n=iso?theater(68,10):"UNIT"+theater(78,3);if(n.empty()){failure="palette-theater-name";return false;}n+=".PAL";if(!files.load(n,bytes)){failure="palette-not-found:"+n;return false;}if(!external::assets::pal(bytes,out,paletteRange_)){failure="palette-format:"+n;return false;}return true;}
std::vector<std::string> spriteNames(uint32_t type,uint32_t pointer,bool overlay=false,bool buildingPart=false,uint32_t parentType=0){
 auto live=external::assets::shapeName(resourceRead,pointer);auto stem=imageName(type);
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
 // All objects in one presentation use one sampled palette per source binding.
 // Revalidate on the next presentation; do not repeat live INI traversal and
 // raw remote palette reads for every instance of an identical unit type.
 PaletteSampleKey key{type,owner,file,overlay,terrain};
 if(presentation_){auto found=presentationPalettes_.find(key);if(found!=presentationPalettes_.end()){out=found->second.colors;lastPaletteLive_=found->second.live;++paletteSampleHits_;return true;}}
 ++paletteSamples_;if(!sampleSpritePalette(type,file,overlay,owner,out,terrain))return false;
 if(presentation_&&presentationPalettes_.size()<4096)presentationPalettes_.emplace(std::move(key),PaletteSample{out,lastPaletteLive_});return true;
}
bool sampleSpritePalette(uint32_t type,const std::string&file,bool overlay,uint32_t owner,std::array<uint32_t,256>&out,bool terrain=false){
 lastPaletteLive_=false;
 if(terrain&&external::lighting::spawnsTiberium(read,type)){
  uint32_t convert=word(0x87F6BC);if(convertPalette(convert,out))return true;
  if(convertedPalette(convert,out)){recoverPalettePrecision("UNITSNO.PAL",out);return true;}
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
 if(!overlay&&!iso&&(custom.empty()||word(type+0xDF0))&&external::voxel::palette(read,type,owner,out)){lastPaletteLive_=true;return true;}
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
double conditionRed(){
 double live=0;uint32_t rules=word(0x8871E0);if(rules>=0x10000&&rules<=0xffffe8f0u&&read(rules+0x1708,&live,sizeof live)&&std::isfinite(live)&&live>=0&&live<=1)return live;
 metadata();auto value=rulesINI.get("AUDIOVISUAL","CONDITIONRED");char*end=nullptr;double threshold=strtod(value.c_str(),&end);if(end&&*end=='%')threshold/=100.;
 bool parsed=end&&end!=value.c_str()&&(!*end||(*end=='%'&&!end[1]));return !parsed||!std::isfinite(threshold)||threshold<0||threshold>1?.25:threshold;
}
int buildingFrame(uint32_t object,uint32_t type){return external::buildings::frame(read,object,type,conditionYellow(),conditionRed());}
void buildingPose(uint32_t object,uint32_t type,external::visual::Pose&pose){
 pose.frame=buildingFrame(object,type);auto state=external::buildings::state(read,object,type,conditionYellow());
 pose.buildingFlags=state.flags;pose.doorFrame=state.doorFrame;
 external::GameView game(read);if(external::buildings::buildup(game,object))pose.variant=-2;else if(pose.variant==-2)pose.variant=-1;
}
struct OverlayPalette {std::string file;std::array<uint32_t,256> colors{};bool live=false;uint64_t revision=1;};
std::map<std::pair<uint32_t,bool>,OverlayPalette> overlayPalettes_;
 assets::LiveINI::IndexDiagnostics liveINIDiagnostics()const{auto a=artINI.indexDiagnostics(),r=rulesINI.indexDiagnostics();return {a.builds+r.builds,a.failures+r.failures,a.failureReuses+r.failureReuses};}
 void beginPresentation(){presentation_=true;artINI.beginPresentation();rulesINI.beginPresentation();presentationPalettes_.clear();paletteSamples_=paletteSampleHits_=0;vpl(voxelRelight_);if(overlayPalettes_.size()>4096)overlayPalettes_.clear();}
bool refreshOverlayPalette(uint32_t type,uint32_t pointer,int frame,bool terrain,bool&changed){
 if(!spriteRelight_&&overlayPalettes_.count({type,terrain})){changed=false;return true;}
 (void)pointer;(void)frame;changed=false;auto found=overlayPalettes_.find({type,terrain});if(found==overlayPalettes_.end())return true;
 auto&binding=found->second;
 return external::livepalette::refresh(binding.colors,binding.live,binding.revision,changed,[&](external::livepalette::Sample&sample){
  bool ok=spritePalette(type,binding.file,true,0,sample.colors,terrain);sample.live=lastPaletteLive_;return ok;
 });
}
bool overlay(uint32_t type,uint32_t pointer,int frame,external::voxel::Image&out,bool terrain=false){std::vector<uint8_t>bytes;std::string file;bool disk=shapeBytes(type,pointer,bytes,file,!terrain);std::array<uint32_t,256>pal{};if(!spritePalette(type,file,true,0,pal,terrain))return false;
 bool paletteLive=lastPaletteLive_,paletteChanged=false;uint64_t paletteRevision=1;
 if(!external::livepalette::refresh(pal,paletteLive,paletteRevision,paletteChanged,[&](external::livepalette::Sample&sample){bool valid=spritePalette(type,file,true,0,sample.colors,terrain);sample.live=lastPaletteLive_;return valid;}))return false;
 overlayPalettes_[{type,terrain}]={file,pal,paletteLive,paletteRevision};
 // Scene overlays use indexed cutouts, not the decoder's translucent preview
 // default. Decode covered pixels opaque before ground composition; trying to
 // undo premultiplication afterwards also loses original palette precision.
 std::string why;bool ok=false;
 if(terrain){const bool animated=(word(type+0x2B3)&255)!=0,spawner=external::lighting::spawnsTiberium(read,type);
  ok=disk?terrainsprite::decode(external::assets::Memory{std::move(bytes)},external::assets::Memory::Base,pal,frame,animated,spawner,out,&why):terrainsprite::decode(resourceRead,pointer,pal,frame,animated,spawner,out,&why);
 }else ok=disk?external::voxel::sprite(external::assets::Memory{std::move(bytes)},external::assets::Memory::Base,pal,out,frame,&why,255):external::voxel::sprite(resourceRead,pointer,pal,out,frame,&why,255);
 if(!ok)failure="overlay-frame/decode:"+file+" frame="+std::to_string(frame)+" "+why;
 if(ok&&!terrain)terrainsprite::overlayDepth(out,(word(type+0x2A9)&255)!=0,(word(type+0x2A8)&255)!=0,(word(type+0x2B3)&255)!=0,(word(type+0x2B5)&255)!=0);
 return ok;}
bool sprite(uint32_t type,uint32_t pointer,const std::array<uint32_t,256>&pal,int frame,external::voxel::Image&out){
 std::vector<uint8_t> bytes;std::string filename;if(!shapeBytes(type,pointer,bytes,filename))return false;
 if(external::assets::shp(std::move(bytes),frame,pal,out))return true;failure="sprite-load/decode:"+filename+" frame="+std::to_string(frame);return false;
}
// A saved bib/custom rubble is a distinct resource. Its fallback must not
// silently substitute the building body when the live SHP is demand-loaded.
int resourceFrames(uint32_t type,uint32_t pointer,bool overlay=false){
 uint8_t header[8]{};if(resourceRead(pointer,header,8)){if(voxel::u16(header)==65535){uint32_t loaded=0;resourceRead(pointer+12,&loaded,4);if(!resourceRead(loaded,header,8))std::memset(header,0,8);}if(voxel::u16(header)==0&&voxel::u16(header+6)>0)return voxel::u16(header+6);}
 std::vector<uint8_t>bytes;std::string file;if(!shapeBytes(type,pointer,bytes,file,overlay)||bytes.size()<8||voxel::u16(bytes.data())!=0)return 0;return voxel::u16(bytes.data()+6);
}
bool overlayDepthShape(voxel::Image&image,unsigned slope,bool legacy){
 if(!slope)return true;if(slope>4)return false;std::vector<uint8_t>bytes;
 if(!files.load("SLOP0"+std::to_string(slope)+"Z."+theater(78,3),bytes)||bytes.size()<32)return true;
 const auto*f=bytes.data()+8;int fx=int16_t(voxel::u16(f)),fy=int16_t(voxel::u16(f+2)),w=voxel::u16(f+4),h=voxel::u16(f+6);uint32_t data=voxel::u32(f+20);
 if(!w||!h||w>2048||h>2048||data>=bytes.size())return true;
 const int ox=-(legacy?30:0)-fx,oy=(legacy?2:0)-fy;
 int l=std::max(0,ox-image.x),t=std::max(0,oy-image.y),right=std::min(image.width,ox-image.x+w),bottom=std::min(image.height,oy-image.y+h);
 int width=right-l,height=bottom-t;if(width<=0||height<=0)return false;
 // 437AAB uses intersection dimensions at the original destination and
 // advances encoded body data only for subsequent viewport clipping.
 voxel::Image next=image;next.width=width;next.height=height;next.pixels.resize(size_t(width)*height);next.indices.resize(next.pixels.size());next.nativeShapeCoverage.assign(next.pixels.size(),1);
 next.nativeDepth.shape=image.nativeExtended;next.nativeDepth.clipTop=0;next.nativeDepth.clipBottom=height;if(image.nativeExtended)next.nativeShape.resize(next.pixels.size());
 for(int y=0;y<height;++y)for(int x=0;x<width;++x){size_t to=size_t(y)*width+x,from=size_t(y)*image.width+x;next.pixels[to]=image.pixels[from];if(image.indices.size()==image.pixels.size())next.indices[to]=image.indices[from];
  if(image.nativeExtended){int64_t at=int64_t(data)+int64_t(image.y+y-oy)*w+image.x+x-ox;next.nativeShape[to]=at>=0&&at<int64_t(bytes.size())?-int(int8_t(bytes[size_t(at)])):0;}}
 image=std::move(next);return true;
}
bool resourceSprite(uint32_t type,uint32_t pointer,const std::array<uint32_t,256>&pal,int frame,voxel::Image&out,bool namedOnly=false,bool overlay=false){
 if(voxel::sprite(resourceRead,pointer,pal,out,frame,nullptr,255))return true;
 auto name=assets::shapeName(resourceRead,pointer);std::vector<uint8_t>bytes;
 if(!name.empty()&&files.load(name,bytes)&&assets::shp(std::move(bytes),frame,pal,out))return true;
 if(namedOnly)return false;std::string filename;if(!shapeBytes(type,pointer,bytes,filename,overlay))return false;return assets::shp(std::move(bytes),frame,pal,out);
}
bool tile(uint32_t type,unsigned sub,const std::array<uint32_t,256>&pal,external::voxel::Image&out){
 const uint32_t resource=word(type+0xA4);const SharedTileKey key{process_,match_,type,resource,sub,files.revision(),pal};
 if(process_){auto cached=sharedTiles_.get(key);
  if(cached){out=*cached;return true;}
 }
 auto retain=[&]{if(process_){
  // Share the byte-bounded LRU policy used by actor images. One new tile must
  // not discard every hot terrain palette/subtile binding in all view workers.
  sharedTiles_.put(key,std::make_shared<const external::voxel::Image>(out));
 }return true;};
 std::vector<uint8_t> bytes;auto filename=text(type+0x2F5,14);std::string reason;
 if(!filename.empty()&&files.load(filename,bytes)&&external::assets::tmp(std::move(bytes),sub,pal,out,&reason))return retain();
 if(external::terrain::tile(resourceRead,resource,sub,pal,out,&reason)){failure.clear();return retain();}
 failure="tile-not-found:"+filename+" memory="+reason;return false;
}
bool shroud(bool fog,int frame,external::voxel::Image&out){
 if(frame<0||frame>46)return false;
 // The game can load these shapes directly from a modded MIX even when the
 // external archive index has not found that archive yet. Prefer its current
 // decoded resource; the disk lookup covers the unloaded-reference case.
 uint32_t loaded=word(fog?0x89E760:0x89E794);
 if(external::shroudassets::loaded(resourceRead,loaded,frame,out))return true;
 if(fog&&external::shroudassets::loaded(resourceRead,word(0x89E790),frame,out))return true;
 std::vector<uint8_t> bytes;
 if(files.load(fog?"FOG.SHP":"SHROUD.SHP",bytes)&&external::shroudassets::archived(std::move(bytes),frame,out))return true;
 failure=fog?"fog-shape-unavailable":"shroud-shape-unavailable";return false;
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
using DebrisModelKey=std::tuple<uint32_t,uint32_t,uint32_t,uint32_t,int,uint64_t>;
inline static std::mutex debrisModelMutex_;inline static std::map<DebrisModelKey,std::weak_ptr<const voxel::Model>>debrisModels_;
struct ActorArt {
 mutable bool sourceKnown=false;mutable nativeimages::Signature sourceSignature;mutable size_t capturedBytes=0;
 external::voxel::Model model;std::shared_ptr<const voxel::Model>sharedGeometry;const voxel::Model&geometry()const{return sharedGeometry?*sharedGeometry:model;}std::vector<uint8_t> shape;std::array<uint32_t,256> colors{};
 // A fresh nonce is assigned only after successful immutable model capture.
 // Copies/moves preserve it; recapture/replacement always creates a new owner.
 gpuvoxel::PacketCache::Geometry geometryIdentity;
 std::string filename;
 uint32_t object=0,id=0,owner=0,parent=0,parentId=0;bool paletteLive=false;uint64_t paletteRevision=1;std::string paletteFilename;
 uint32_t pointer=0,type=0;float turretOffset=0;bool voxel=false;int kind=-1,fw=1,fh=1,moveX=0,moveY=0,drawY=0,zAdjust=0;bool flat=false,shadow=true;
 std::shared_ptr<const external::voxel::Image> depthMask;bool depthMaskBiased=false;
 std::array<BuildingPart,external::buildings::PartCount> buildingParts;
 struct Gun {std::shared_ptr<const voxel::Model>geometry;gpuvoxel::PacketCache::Geometry identity;int frames=1;};std::array<Gun,2>guns;int gunX=0,gunY=0,gunZ=0;float gunOffset=0;bool hasGuns()const{return bool(guns[0].geometry)||bool(guns[1].geometry);}
};
static bool liveVoxel(const ActorArt&a){return a.voxel||a.hasGuns();}
 bool gpuVoxel(const ActorArt&art,double angle,double turret,double pitch,bool projectile,int scale,const voxelposture::State*posture,bool shadow,voxel::Image&out,int bodyFrame=0,int turretFrame=0,double gunPitch=0,std::array<float,2>recoil={}){
  if(!shadow)gpuVplSnapshot();
  const bool indexed=!shadow&&gpuVplSnapshot_&&voxelGeometry_.indexed(art.geometry(),art.geometryIdentity);
  std::array<uint32_t,256> neutral{};neutral.fill(0xffffffffu);const auto&colors=(indexed||shadow)?neutral:art.colors;
  auto key=gpuvoxel::PacketCache::key(art.geometryIdentity,shadow?std::shared_ptr<const gpuvoxel::PacketCache::VplSnapshot>{}:gpuVplSnapshot_,colors,angle,turret,pitch,art.turretOffset,scale,projectile,shadow,posture,bodyFrame,turretFrame,gunPitch,recoil);
  auto packet=gpuPackets_.find(key);
  if(!packet){auto next=std::make_shared<gpuvoxel::Prepared>();
   std::shared_ptr<const voxel::Model> animated;const voxel::Model*model=&art.geometry();
   if(bodyFrame||turretFrame){animated=voxelGeometry_.animated(*model,art.geometryIdentity,bodyFrame,turretFrame);if(!animated)return false;model=animated.get();}
   if(shadow){voxel::Model flattened=*model;for(auto&face:flattened.faces)for(auto&p:face.p){if(art.kind==voxelanims::Kind&&posture)p=voxel::posturePoint(posture->ramp,p);p.z=0;}
    if(!gpuvoxel::prepare(flattened,angle,colors,*next,turret,art.turretOffset,nullptr,255,0,false,scale,art.kind==voxelanims::Kind?nullptr:posture))return false;
   }else if(!gpuvoxel::prepare(*model,angle,colors,*next,turret,art.turretOffset,&voxelLight,255,pitch,projectile,scale,posture,gunPitch,recoil))return false;
   packet=next;gpuPackets_.insert(std::move(key),packet);
  }
  out={};out.rasterScale=packet->rasterScale;out.width=packet->width;out.height=packet->height;out.x=packet->x;out.y=packet->y;out.depthBottom=packet->depthBottom;out.objectPalette=true;out.gpuRaster=std::move(packet);
  if(indexed){auto found=gpuPalettes_.find(art.colors);if(found!=gpuPalettes_.end())out.sourcePalette=found->second.lock();
   if(!out.sourcePalette){for(auto i=gpuPalettes_.begin();i!=gpuPalettes_.end();)if(i->second.expired())i=gpuPalettes_.erase(i);else ++i;
    auto palette=std::make_shared<std::vector<uint32_t>>(256);for(size_t n=0;n<256;++n)(*palette)[n]=0xff000000u|(art.colors[n]&0xffffffu);out.sourcePalette=palette;
    if(gpuPalettes_.size()<1024)gpuPalettes_[art.colors]=std::move(palette);
   }
  }return true;
 }
static size_t cost(const ActorArt&a){if(a.capturedBytes)return a.capturedBytes;if(a.sharedGeometry)return sizeof(ActorArt);size_t n=a.shape.size()+a.model.faces.size()*sizeof(external::voxel::Face);std::set<const voxel::Motion*>seen;for(const auto&face:a.model.faces)if(face.motion&&seen.insert(face.motion.get()).second)n+=face.motion->changes.capacity()*48;for(const auto&part:a.buildingParts)n+=part.shape.size();for(const auto&gun:a.guns)if(gun.geometry){n+=gun.geometry->faces.size()*sizeof(voxel::Face);for(const auto&face:gun.geometry->faces)if(face.motion&&seen.insert(face.motion.get()).second)n+=face.motion->changes.capacity()*48;}a.capturedBytes=std::max<size_t>(1,n);return n;}
static int integer(const std::string&s,int fallback=0){if(s.empty())return fallback;char*end=nullptr;long n=strtol(s.c_str(),&end,10);return end!=s.c_str()&&!*end?int(std::clamp(n,-8192l,8192l)):fallback;}
void captureBuildingParts(uint32_t type,ActorArt&art){
 for(int i=0;i<external::buildings::PartCount;++i){auto&part=art.buildingParts[i];part.pointer=word(type+external::buildings::partOffsets[i]);if(part.pointer<0x10000){part.pointer=0;continue;}
  auto live=external::assets::shapeName(resourceRead,part.pointer);auto stem=i==external::buildings::Bib?artValue(type,"BIBSHAPE"):std::string{};
  if(stem.size()>4&&external::assets::name(stem.substr(stem.size()-4))==".SHP")stem.resize(stem.size()-4);
  // A body Image name is not a fallback for a factory door. Loaded parts can
  // decode directly from memory; unloaded references retain their real name.
  auto names=external::assets::spriteNames(stem,live,theater(78,3),theater(86,1),(word(type+0x22C)&255)!=0,(word(type+0x237)&255)!=0,false,true);
  part.filename=live;for(const auto&name:names)if(files.load(name,part.shape)){part.filename=name;break;}
 }
}
bool animationPalette(uint32_t p,uint32_t type,uint32_t parent,uint32_t owner,int x,int y,std::array<uint32_t,256>&out){
 lastPaletteLive_=false;
 auto source=external::lighting::animationSource(read,p,type,external::lighting::cellAt(read,x,y));
 bool colors=source.tintable&&convertPalette(source.converter,out);
 auto custom=text(p+0xDC,32);if(custom.empty())custom=artValue(type,"PALETTE");
 if(!colors&&!custom.empty()){custom=external::assets::name(custom);colors=custom.size()>4&&custom.substr(custom.size()-4)==".PAL"?paletteFile(custom,out):(paletteFile(custom+theater(78,3)+".PAL",out)||paletteFile(custom+".PAL",out));}
 else if(!colors&&parent>=0x10000&&(word(p+0x118)&255))colors=spritePalette(word(parent+0x520),"",false,owner,out);
 else if(!colors)colors=paletteFile("ANIM.PAL",out);
 if(!colors)return false;
 return true;
}
bool captureAnimation(uint32_t p,uint32_t type,uint32_t id,uint32_t parent,uint32_t parentId,ActorArt&out){
 external::animations::State state;
 if(!external::animations::capture(read,p,parent,parentId,state)||state.id!=id||state.type!=type){failure="animation-identity/slot";return false;}
 ActorArt next;next.kind=5;next.type=type;next.pointer=word(type+0xA4);next.flat=(word(type+0x369)&255)!=0;next.shadow=(word(type+0x372)&255)!=0;
 next.drawY=int32_t(word(type+0x344));if(std::abs(int64_t(next.drawY))>4096)return false;
 bool loaded=shapeBytes(type,next.pointer,next.shape,next.filename,false,true,word(parent+0x520));
 if(!loaded)next.filename=external::assets::shapeName(resourceRead,next.pointer);
 if(!animationPalette(p,type,parent,state.owner,state.x,state.y,next.colors))return false;
 next.object=p;next.id=id;next.owner=state.owner;next.parent=parent;next.parentId=parentId;next.paletteLive=lastPaletteLive_;
 // A theater SHP already loaded by the engine remains usable when its archive
 // cannot be resolved on disk. Frame zero is valid.
 if(!loaded){external::voxel::Image check;std::string why;
  if(!external::voxel::sprite(resourceRead,next.pointer,next.colors,check,state.frame,&why,255)){failure="animation-memory:"+why;return false;}}
 external::animations::State after;
 if(!external::animations::capture(read,p,parent,parentId,after)||after.id!=id||after.type!=type||after.owner!=state.owner){failure="animation-changed";return false;}
 bool paletteChanged=false;if(!refreshActorPalette(next,paletteChanged))return false;
 failure.clear();out=std::move(next);return true;
}
bool debrisPalette(uint32_t owner,std::array<uint32_t,256>&out){lastPaletteLive_=false;if(owner>=0x10000){int index=int(word(owner+0x16054)),count=int(word(0xB054E0));uint32_t items=word(0xB054D4),scheme=index>=0&&index<count&&count<=512?word(items+index*4):0;uint8_t raw[768]{};if(scheme>=0x10000&&read(scheme+4,raw,sizeof raw)){for(int i=0;i<256;++i)out[i]=0xFF000000u|(uint32_t(raw[i*3])<<16)|(uint32_t(raw[i*3+1])<<8)|raw[i*3+2];out[0]=0;lastPaletteLive_=true;return true;}}return convertPalette(word(0x87F6BC),out)||paletteFile("UNITSNO.PAL",out);}
bool captureVoxelAnimation(uint32_t p,uint32_t type,uint32_t id,ActorArt&out){voxelanims::State state;if(!voxelanims::capture(read,p,state)||state.id!=id||state.type!=type)return false;ActorArt next;next.object=p;next.type=type;next.id=id;next.owner=state.owner;next.kind=voxelanims::Kind;next.voxel=true;next.shadow=true;
 DebrisModelKey key{process_,match_,type,word(type+0xB0),state.section,files.revision()};{std::lock_guard<std::mutex>lock(debrisModelMutex_);auto found=debrisModels_.find(key);if(found!=debrisModels_.end())next.sharedGeometry=found->second.lock();}
 if(!next.sharedGeometry){voxel::Model model;bool loaded=voxel::captureRaw(resourceRead,word(type+0xB0),model);if(!loaded){std::vector<uint8_t>v;auto stem=imageName(type);loaded=!stem.empty()&&files.load(stem+".VXL",v)&&assets::rawVxl(v,model);}if(!loaded)return false;model.faces.erase(std::remove_if(model.faces.begin(),model.faces.end(),[&](const auto&f){return f.section!=state.section;}),model.faces.end());if(model.faces.empty())return false;next.sharedGeometry=std::make_shared<const voxel::Model>(std::move(model));std::lock_guard<std::mutex>lock(debrisModelMutex_);if(debrisModels_.size()>=4096)for(auto i=debrisModels_.begin();i!=debrisModels_.end();)if(i->second.expired())i=debrisModels_.erase(i);else ++i;if(debrisModels_.size()>=4096)debrisModels_.erase(debrisModels_.begin());debrisModels_[key]=next.sharedGeometry;}
 if(!debrisPalette(state.owner,next.colors))return false;next.paletteLive=lastPaletteLive_;next.geometryIdentity=std::make_shared<const uint8_t>(0);out=std::move(next);return true;}

bool effectAlpha(uint32_t pointer,voxel::Image&out){std::array<uint32_t,256>colors{};for(unsigned i=0;i<256;++i)colors[i]=0xFF000000u|(i<<16)|(i<<8)|i;return voxel::sprite(resourceRead,pointer,colors,out,0,nullptr,255);}
bool particlePalette(std::array<uint32_t,256>&out){lastPaletteLive_=false;return convertPalette(word(0x87F6C0),out)||paletteFile("ANIM.PAL",out);}
bool captureParticle(uint32_t p,uint32_t type,uint32_t id,ActorArt&out){
 external::particles::State state;if(!external::particles::capture(read,p,state)||state.id!=id||state.type!=type)return false;
 ActorArt next;next.kind=external::particles::Kind;next.type=type;next.object=p;next.id=id;next.pointer=word(type+0xA4);next.flat=true;next.shadow=false;
 const bool loaded=shapeBytes(type,next.pointer,next.shape,next.filename,false,true);if(!particlePalette(next.colors))return false;next.paletteLive=lastPaletteLive_;
 if(!loaded){external::voxel::Image check;if(!external::voxel::sprite(resourceRead,next.pointer,next.colors,check,state.frame,nullptr,255))return false;}
 external::particles::State after;if(!external::particles::capture(read,p,after)||after.id!=id||after.type!=type)return false;out=std::move(next);return true;
}
bool projectilePalette(const external::projectiles::State&state,std::array<uint32_t,256>&out){
 lastPaletteLive_=false;
 bool colors=convertPalette(state.converter,out);
 if(!colors&&state.palette==external::projectiles::SchemePalette){
  uint32_t items=word(0xB054D4),scheme=0,again=0;int count=int(word(0xB054E0));uint8_t raw[768]{};
  if(items>=0x10000&&state.colorIndex>=0&&state.colorIndex<count&&count<=512&&uint64_t(items)+uint64_t(count)*4<=0x100000000ull&&
     read(items+state.colorIndex*4,&scheme,4)&&scheme>=0x10000&&uint64_t(scheme)+772<=0x100000000ull&&read(scheme+4,raw,sizeof raw)&&
     read(items+state.colorIndex*4,&again,4)&&again==scheme&&word(scheme+0x30C)==state.converter){
   for(int i=0;i<256;++i)out[i]=0xff000000u|(uint32_t(raw[i*3])<<16)|(uint32_t(raw[i*3+1])<<8)|raw[i*3+2];out[0]=0;lastPaletteLive_=true;colors=true;
  }
 }
 if(!colors&&word(state.converter)==0x7E5358&&word(state.converter+4)==2){
  int shades=int(word(state.converter+0x16C));uint32_t palette=word(state.converter+0x174);uint16_t raw[256]{};
  if(shades>=1&&shades<=256&&palette>=0x10000&&uint64_t(palette)+sizeof raw<=0x100000000ull&&read(palette,raw,sizeof raw)&&word(state.converter+0x174)==palette){
   for(int i=0;i<256;++i){unsigned c=raw[i],r=(c>>11)&31,g=(c>>5)&63,b=c&31;out[i]=0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}out[0]=0;lastPaletteLive_=true;colors=true;
   if(state.palette!=external::projectiles::SchemePalette)recoverPalettePrecision(state.palette==external::projectiles::AnimationPalette?"ANIM.PAL":"PALETTE.PAL",out);
  }
 }
 if(!colors&&state.palette!=external::projectiles::SchemePalette)colors=paletteFile(state.palette==external::projectiles::AnimationPalette?"ANIM.PAL":"PALETTE.PAL",out);
 if(!colors){failure="projectile-palette";return false;}
 return true;
}
bool captureProjectile(uint32_t p,uint32_t type,uint32_t id,ActorArt&out){
 external::projectiles::State state;if(!external::projectiles::capture(read,p,state)||state.type!=type||state.id!=id){failure="projectile-identity";return false;}
 ActorArt next;next.kind=external::projectiles::Kind;next.type=type;next.voxel=state.voxel;next.flat=state.voxel?state.flat:true;next.shadow=state.shadow&&!state.voxel;next.pointer=word(type+0xA4);
 // Resource/ANIM drawers are plain ConvertClass. Their native neutral row
 // is authoritative; +188 belongs only to LightConvertClass, never these.
 if(!projectilePalette(state,next.colors))return false;
 next.object=p;next.id=id;next.owner=state.owner;next.paletteLive=lastPaletteLive_;
 if(state.voxel){
  bool loaded=external::voxel::capturePart(resourceRead,word(type+0xB0),word(type+0xB4),next.model);
  if(!loaded){std::vector<uint8_t>v,h;auto stem=imageName(type);loaded=!stem.empty()&&files.load(stem+".VXL",v)&&files.load(stem+".HVA",h)&&external::assets::vxl(v,h,next.model);}
  if(!loaded){failure="projectile-voxel";return false;}
 }else{
  bool loaded=shapeBytes(type,next.pointer,next.shape,next.filename);if(!loaded)next.filename=external::assets::shapeName(resourceRead,next.pointer);
  if(!loaded){external::voxel::Image check;std::string reason;if(!external::voxel::sprite(resourceRead,next.pointer,next.colors,check,state.pose.frame,&reason,255)){failure="projectile-sprite:"+reason;return false;}}
 }
 external::projectiles::State after;if(!external::projectiles::capture(read,p,after)||after.type!=state.type||after.id!=state.id||after.converter!=state.converter||after.palette!=state.palette){failure="projectile-changed";return false;}
 bool paletteChanged=false;if(!refreshActorPalette(next,paletteChanged))return false;
 if(next.voxel)next.geometryIdentity=std::make_shared<const uint8_t>(0);
 failure.clear();out=std::move(next);return true;
}
bool captureActor(uint32_t p,uint32_t type,uint32_t id,ActorArt&out,const external::visual::Pose*expected=nullptr,int displayKind=-1,uint32_t displayOwner=0){
 failure.clear();if(word(p+0x10)!=id){failure="actor-identity";return false;}ActorArt next;external::GameView game(read);const int actualKind=game.kind(p);const uint32_t actualType=game.type(p,actualKind),actualOwner=word(p+0x21C);if(actualKind<0||(displayKind<0&&actualType!=type))return false;next.kind=displayKind<0?actualKind:displayKind;next.type=type;uint32_t owner=displayKind<0?actualOwner:displayOwner;next.object=p;next.id=id;next.owner=owner;
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
   buildingMaskBiased=external::voxel::sprite(resourceRead,word(0x89DDBC),dummy,*mask,0,nullptr,255);
   if(buildingMaskBiased||((files.load("BUILDNGZ.SHA",b)||files.load("BUILDNGZ.SHP",b))&&external::assets::shp(std::move(b),0,dummy,*mask)))buildingMask=std::move(mask);}
  if(next.fw<8){next.depthMask=buildingMask;next.depthMaskBiased=buildingMaskBiased;} // native 43D787 skips the cone for width >= 8
 }

 auto stem=text(type+0x1F8,25);if(stem.empty())stem=text(type+0x24,24);
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 auto part=[&](const std::string&suffix,uint32_t at,bool turret){
  external::voxel::Model model;std::vector<uint8_t> v,h;
  bool ok=!stem.empty()&&files.load(stem+suffix+".VXL",v)&&files.load(stem+suffix+".HVA",h)&&external::assets::vxl(v,h,model,0,float(sx),float(sy));
  if(!ok)ok=external::voxel::capturePart(resourceRead,word(at),word(at+4),model,0,float(sx),float(sy));
  if(ok){for(auto&f:model.faces){f.turret=turret;f.part=turret?(suffix=="BARL"?2:1):0;}next.model.faces.insert(next.model.faces.end(),model.faces.begin(),model.faces.end());}return ok;
 };
 if(next.kind!=3&&part("",type+0xB0,false)){
  if(!spritePalette(type,"",false,owner,next.colors))return false;next.paletteLive=lastPaletteLive_;
  next.voxel=true;int offset=int32_t(word(type+0x720));if(offset<-4096||offset>4096)return false;next.turretOffset=float(offset)*12/256*float(sx);
  int variant=-1;if(int(word(type+0x808))>0&&!uint8_t(word(type+0xCD5)))variant=int(word(p+0x124));
  if(variant>=0&&variant<18){
   // Native 73B90E and 70DCF0 select ExtraBarrels/ExtraTurrets by
   // CurrentTurretNumber (+124), not CurrentWeaponNumber (+138).
   for(uint32_t at:{type+0xC8+uint32_t(variant)*8,type+0x158+uint32_t(variant)*8}){
    external::voxel::Model component;if(external::voxel::capturePart(resourceRead,word(at),word(at+4),component,0,float(sx),float(sy))){for(auto&f:component.faces){f.turret=true;f.part=at<type+0x158?1:2;}next.model.faces.insert(next.model.faces.end(),component.faces.begin(),component.faces.end());}
   }
  }else{part("TUR",type+0xB8,true);part("BARL",type+0xC0,true);}
 }else{
  bool buildup=next.kind==3&&(expected?expected->variant==-2:external::buildings::buildup(game,p));
  next.pointer=word(type+(buildup?0xE00:0xA4));std::string filename;bool loaded=false;
  if(buildup){auto stem=text(type+0xE5C,16);if(stem.empty())stem=artValue(type,"Buildup");
   auto names=external::assets::spriteNames(stem,external::assets::shapeName(resourceRead,next.pointer),theater(78,3),theater(86,1),(word(type+0x22C)&255)!=0,(word(type+0x237)&255)!=0,false,true);
   for(const auto&name:names)if(files.load(name,next.shape)){filename=name;loaded=true;break;}
  }else loaded=shapeBytes(type,next.pointer,next.shape,filename,false,next.kind==3);next.filename=filename;
  if(!loaded)filename=external::assets::shapeName(resourceRead,next.pointer);
  bool terrainPalette=next.kind==3&&(word(type+0x1702)&255);
  if(terrainPalette?!palette(true,next.colors):!spritePalette(type,filename,false,owner,next.colors))return false;next.paletteFilename=filename;next.paletteLive=lastPaletteLive_;
  if(!loaded){external::voxel::Image check;if(!external::voxel::sprite(resourceRead,next.pointer,next.colors,check)){failure="sprite-not-found/unloaded:"+(filename.empty()?stem:filename);return false;}}
 }
 if(next.kind==3){captureBuildingParts(type,next);
  if(game.template get<uint8_t>(type+0x16C5)){
   next.gunX=game.template get<int>(type+0x11E0);next.gunY=game.template get<int>(type+0x11E4);next.gunZ=game.template get<int>(type+0x11E8);next.gunOffset=float(int32_t(word(type+0x720))/8);
   if(std::abs(int64_t(next.gunX))>8192||std::abs(int64_t(next.gunY))>8192||std::abs(int64_t(next.gunZ))>8192||std::abs(next.gunOffset)>4096)return false;
   for(unsigned n=0;n<2;++n){uint32_t slot=type+0xB8+n*8,vxl=word(slot),hva=word(slot+4);if(vxl<0x10000||hva<0x10000)continue;
    voxel::Model model;if(!voxel::capturePart(resourceRead,vxl,hva,model,0,float(sx),float(sy))){failure="building-gun/live-slot";return false;}
    const int count=game.template get<int>(hva+8,1);if(count<1||count>4096)return false;
    for(auto&face:model.faces){face.turret=true;face.part=uint8_t(n+1);}next.guns[n]={std::make_shared<const voxel::Model>(std::move(model)),std::make_shared<const uint8_t>(0),count};
   }
  }
 }
 if(word(p+0x10)!=id||word(p+0x21C)!=actualOwner||game.type(p,actualKind)!=actualType)return false;bool paletteChanged=false;if(!refreshActorPalette(next,paletteChanged))return false;if(next.voxel)next.geometryIdentity=std::make_shared<const uint8_t>(0);out=std::move(next);return true;
}
template<class Capture>bool paletteActorCapture(const ActorArt&art,Capture capture){
 // These already-known object/type bindings are revalidated by capture. The
 // wrapper preserves every repeat read, including its final identity checks;
 // it lives for only ONE of livepalette::refresh's two independent resolves.
 fieldreads::First<Read> fields(read);
 if(art.kind==5){fields.block(art.object,0x1A0);fields.block(art.type+0x2B4,0xB8);
  if(art.parent){fields.block(art.parent,0x224);fields.block(art.parent+0x55C,0x8C);}
 }else if(art.kind==external::particles::Kind){fields.block(art.object,0x130);fields.block(art.type+0x2B0,0x68);
 }else if(art.kind==external::projectiles::Kind){fields.block(art.object,0x15C);fields.block(art.type,0x2F8);}
 return capture(fields.view());
}
bool refreshActorPalette(ActorArt&art,bool&changed){
 changed=false;if(!(art.voxel?voxelRelight_:spriteRelight_))return true;
 return external::livepalette::refresh(art.colors,art.paletteLive,art.paletteRevision,changed,[&](external::livepalette::Sample&sample){
  bool ok=false;
  if(art.kind==5){external::animations::State state;
   if(!paletteActorCapture(art,[&](auto sample){return external::animations::capture(sample,art.object,art.parent,art.parentId,state);})||state.id!=art.id||state.type!=art.type||state.owner!=art.owner)return false;
   ok=animationPalette(art.object,art.type,art.parent,state.owner,state.x,state.y,sample.colors);
  }else if(art.kind==external::particles::Kind){external::particles::State state;
   if(!paletteActorCapture(art,[&](auto sample){return external::particles::capture(sample,art.object,state);})||state.id!=art.id||state.type!=art.type)return false;
   ok=particlePalette(sample.colors);
  }else if(art.kind==external::voxelanims::Kind){external::voxelanims::State state;if(!external::voxelanims::capture(read,art.object,state)||state.id!=art.id||state.type!=art.type)return false;ok=debrisPalette(state.owner,sample.colors);
  }else if(art.kind==external::projectiles::Kind){external::projectiles::State state;
   if(!paletteActorCapture(art,[&](auto sample){return external::projectiles::capture(sample,art.object,state);})||state.id!=art.id||state.type!=art.type)return false;
   ok=projectilePalette(state,sample.colors);
  }else{
   bool terrainPalette=art.kind==3&&!art.voxel&&(word(art.type+0x1702)&255);
   ok=terrainPalette?palette(true,sample.colors):spritePalette(art.type,art.paletteFilename,false,art.owner,sample.colors);
  }
  sample.live=lastPaletteLive_;return ok;
 });
}
// Share already drawn native materials across main/watch/map contexts. Source
// fingerprints are computed once per immutable capture, not once per pose.
bool renderSharedActor(const ActorArt&art,const external::visual::Pose&pose,std::shared_ptr<const external::voxel::Image>&out){
 if((art.voxel||art.hasGuns())&&!voxelLightingAvailable())return false;
 bool cacheable=process_&&art.id&&(art.voxel?bool(art.geometryIdentity):!art.shape.empty());
 if(art.kind==3)for(const auto&part:art.buildingParts)if(part.pointer&&part.shape.empty())cacheable=false;
 if(cacheable&&!art.sourceKnown){auto&h=art.sourceSignature;std::set<const voxel::Motion*> motions;
  for(const auto&f:art.geometry().faces){for(const auto&p:f.p){h.add(p.x);h.add(p.y);h.add(p.z);}h.add(f.normal.x);h.add(f.normal.y);h.add(f.normal.z);h.add(f.color);h.add(f.normalIndex);h.add(f.normalType);h.add(f.turret);h.add(f.part);h.add(f.hasNormal);h.add(f.section);if(f.motion&&motions.insert(f.motion.get()).second)for(const auto&m:f.motion->changes)for(float v:m)h.add(v);}
  for(const auto&gun:art.guns)if(gun.geometry){h.add(gun.frames);std::set<const voxel::Motion*>seen;for(const auto&f:gun.geometry->faces){for(const auto&p:f.p){h.add(p.x);h.add(p.y);h.add(p.z);}h.add(f.color);h.add(f.normalIndex);h.add(f.normalType);h.add(f.part);h.add(f.section);if(f.motion&&seen.insert(f.motion.get()).second)for(const auto&m:f.motion->changes)for(float v:m)h.add(v);}}
  h.add(art.gunX);h.add(art.gunY);h.add(art.gunZ);h.add(art.gunOffset);
  h.bytes(art.shape.data(),art.shape.size());for(const auto&part:art.buildingParts){h.add(part.pointer);h.bytes(part.shape.data(),part.shape.size());}
  for(int v:{art.kind,art.fw,art.fh,art.moveX,art.moveY,art.drawY,art.zAdjust})h.add(v);
  h.add(art.turretOffset);h.add(art.flat);h.add(art.shadow);h.add(art.depthMaskBiased);
  if(art.depthMask){h.bytes(art.depthMask->pixels.data(),art.depthMask->pixels.size()*4);h.bytes(art.depthMask->depth.data(),art.depthMask->depth.size()*4);h.add(art.depthMask->width);h.add(art.depthMask->height);h.add(art.depthMask->x);h.add(art.depthMask->y);}
  art.sourceKnown=true;
 }
 // Fully captured pure SHP is decoded only from immutable bytes and pose.
 // Keep capture/liveness checks per actor, but share its identical material
 // across transient animation/particle/projectile instances as well. Mixed
 // or live-pointer art retains the existing identity-sensitive cache key.
 const bool capturedSprite=!art.voxel&&!art.hasGuns()&&!art.shape.empty()&&art.kind>=5&&art.kind!=voxelanims::Kind;
 const bool instanceKey=art.kind>=5&&art.kind!=voxelanims::Kind&&!capturedSprite;
 nativeimages::Key key;if(cacheable){key.palette=art.colors;key.values={process_,match_,files.revision(),instanceKey?art.object:0,instanceKey?art.id:0,art.type,art.owner,art.parent,art.parentId,art.sourceSignature.a,art.sourceSignature.b,(art.voxel||art.hasGuns())?voxelLight.signature():0,
  uint32_t(pose.body)|(uint64_t(pose.turret)<<8)|(uint64_t(uint32_t(pose.frame))<<16),uint32_t(pose.variant),uint32_t(pose.buildingFlags),uint32_t(pose.doorFrame),uint32_t(pose.turretFrame),uint32_t(pose.bodyFine),uint32_t(pose.turretFine),external::visual::postureSignature(pose),(art.voxel||art.hasGuns())?uint32_t(presentationRasterScale()):1,(art.voxel||art.hasGuns())?uint64_t(gpuPresentation_):0,uint64_t(art.pointer),uint32_t(pose.hvaBody)|(uint64_t(uint32_t(pose.hvaTurret))<<32)};
  if(auto image=nativeimages::bank().get(key)){out=std::move(image);return true;}
 }
 external::voxel::Image image;if(!renderActor(art,pose,image))return false;external::camera::opaque(image);out=std::make_shared<const external::voxel::Image>(std::move(image));if(cacheable)nativeimages::bank().put(std::move(key),out);return true;
}
bool renderActor(const ActorArt&art,const external::visual::Pose&pose,external::voxel::Image&out){
 auto angle=external::visual::fineRadians(pose.bodyFine,pose.body),turret=external::visual::fineRadians(pose.turretFine,pose.turret);
 if(art.voxel){
  if(!pose.gunPoseValid){failure="voxel-current-recoil-unavailable";return false;}
  const double gunPitch=-(int(external::visual::bucket(pose.rawBarrel))-8)*(2*external::visual::Pi/32);
  const int rasterScale=presentationRasterScale();if(!rasterScale){failure="voxel-presentation-resolution-unsupported";return false;}
  if(voxelRelight_&&presentation_&&!presentationVplFresh_){failure="voxel-current-vpl-unavailable";return false;}
  if(!presentation_)vpl();bool projectile=art.kind==external::projectiles::Kind;
  if(projectile&&!voxelLight.ready()){failure="projectile-vpl-unavailable";return false;}
  if(!pose.posture.available){failure="voxel-current-posture-unavailable";return false;}
  const int bodyFrame=pose.hvaBody,turretFrame=bodyFrame?bodyFrame:pose.hvaTurret;if(bodyFrame<0||turretFrame<0)return false;
  if(gpuPresentation_){
   if(!gpuVoxel(art,angle,projectile?std::numeric_limits<double>::quiet_NaN():turret,projectile?turret:0,projectile,rasterScale,&pose.posture,false,out,bodyFrame,turretFrame,projectile?0:gunPitch,pose.recoil))return false;
  }else{std::shared_ptr<const voxel::Model> animated;const voxel::Model*model=&art.geometry();if(bodyFrame||turretFrame){animated=voxelGeometry_.animated(*model,art.geometryIdentity,bodyFrame,turretFrame);if(!animated)return false;model=animated.get();}if(!external::voxel::render(*model,angle,art.colors,out,projectile?std::numeric_limits<double>::quiet_NaN():turret,art.turretOffset,&voxelLight,255,projectile?turret:0,projectile,rasterScale,&pose.posture,projectile?0:gunPitch,pose.recoil))return false;}
  // The model Z buffer resolves faces inside the cached voxel only. The game
  // submits that cached image through the same standing shape Z test as SHP.
  // Orthographic model depth is not in the terrain/BUILDNGZ buffer's units.
  std::vector<float>().swap(out.depth);out.writeDepth=false;out.nativeDepth.enabled=true;out.nativeDepth.voxel=true;out.nativeDepth.gradient=nativesurface::Gradient::Vertical;if(projectile){out.flat=art.flat;if(art.flat)out.nativeDepth.gradient=nativesurface::Gradient::Flat;
   // 46B1A5/490B90: non-flat projectile flags 0800 select a blitter
   // without a Z read or write. Flat 2800 keeps the ground Z read.
   if(!art.flat){if(gpuPresentation_)out.skipZ=true;else out.depth.assign(out.pixels.size(),std::numeric_limits<float>::quiet_NaN());}
  }
  if(art.shadow){external::voxel::Image shadow;bool drawn=false;
   auto shadowPosture=art.kind==voxelanims::Kind?pose.posture:external::voxelshadow::projection(pose.shadow);
   if(!shadowPosture.available){failure="voxel-current-shadow-unavailable";return false;}
   if(gpuPresentation_)drawn=gpuVoxel(art,angle,turret,0,false,rasterScale,&shadowPosture,true,shadow,bodyFrame,turretFrame);
   else{voxel::Model ground;if(bodyFrame||turretFrame){auto animated=voxelGeometry_.animated(art.geometry(),art.geometryIdentity,bodyFrame,turretFrame);if(!animated)return false;ground=*animated;}else ground=art.geometry();for(auto&face:ground.faces)for(auto&p:face.p){if(art.kind==voxelanims::Kind)p=voxel::posturePoint(pose.posture.ramp,p);p.z=0;}drawn=external::voxel::render(ground,angle,art.colors,shadow,turret,art.turretOffset,nullptr,255,0,false,rasterScale,art.kind==voxelanims::Kind?nullptr:&shadowPosture);}
   if(drawn){shadow.x+=(art.kind==voxelanims::Kind?0:3)*shadow.rasterScale;shadow.shadow=true;shadow.flat=true;shadow.writeDepth=false;shadow.depthBias=art.kind==voxelanims::Kind?-2:2;shadow.nativeDepth.enabled=true;shadow.nativeDepth.gradient=nativesurface::Gradient::Flat;shadow.nativeDepth.adjust=-int(shadow.depthBias);std::vector<float>().swap(shadow.depth);out.attachments.push_back(std::move(shadow));}}
  return true;
 }
 if(pose.frame<0){failure="sprite-sequence";return false;}
 auto decode=[&](const std::vector<uint8_t>&shape,uint32_t pointer,int frame,external::voxel::Image&image,std::string&why){
  auto bytes=[&](uint32_t at,void*p,size_t n){if(at<external::assets::Memory::Base||uint64_t(at-external::assets::Memory::Base)+n>shape.size())return false;memcpy(p,shape.data()+at-external::assets::Memory::Base,n);return true;};
  return !shape.empty()?external::voxel::sprite(bytes,external::assets::Memory::Base,art.colors,image,frame,&why,255):external::voxel::sprite(resourceRead,pointer,art.colors,image,frame,&why,255);
 };
 auto frames=[&](const std::vector<uint8_t>&shape,uint32_t pointer){uint8_t head[8]{};if(!shape.empty()){if(shape.size()<8)return 0;memcpy(head,shape.data(),8);}
  else{if(pointer<0x10000||!resourceRead(pointer,head,8))return 0;if(external::voxel::u16(head)==65535){uint32_t loaded=0;if(!resourceRead(pointer+12,&loaded,4))return 0;pointer=loaded;if(pointer<0x10000||!resourceRead(pointer,head,8))return 0;}}
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
 out.y+=art.drawY;out.objectPalette=true;out.flat=art.flat;out.writeDepth=art.kind==3;out.depthBias=bias;out.nativeDepth.enabled=true;out.nativeDepth.gradient=art.flat?nativesurface::Gradient::Flat:nativesurface::Gradient::Vertical;out.nativeDepth.adjust=-int(bias);
 if(art.kind==3&&!out.flat&&art.depthMask)external::camera::buildingDepth(out,*art.depthMask,art.fw,art.fh,art.moveX,art.moveY,art.depthMaskBiased);
 auto shadow=[&](const std::vector<uint8_t>&bytes,uint32_t at,int displayed,external::voxel::Image&image,bool write,float shadowBias){int n=frames(bytes,at);if(!art.shadow||n<2||displayed<0||displayed>=n/2)return;external::voxel::Image part;
  if(decode(bytes,at,displayed+n/2,part,why)){part.y+=art.drawY;part.shadow=true;part.flat=true;part.writeDepth=write;part.nativeDepth.enabled=true;part.nativeDepth.gradient=nativesurface::Gradient::Flat;part.nativeDepth.adjust=-int(shadowBias);image.shadowsAfter=art.kind==3||write;part.depthBias=shadowBias;image.attachments.push_back(std::move(part));}};
 // DrawObject's shadow branch recomputes its ground plane independently of
 // NormalZAdjust and the -20/-40 production-body adjustment (706469/70653E).
 if(art.kind==external::projectiles::Kind){if(art.shadow){auto ground=out;ground.attachments.clear();ground.shadow=true;ground.flat=true;ground.writeDepth=false;ground.depthBias=-20;ground.nativeDepth.gradient=nativesurface::Gradient::Flat;ground.nativeDepth.adjust=20;ground.airborneShadow=true;out.attachments.push_back(std::move(ground));}}
 else shadow(*shape,pointer,frame,out,art.kind==3,art.kind==3?2.f:out.depthBias+(art.kind==5?0:2));
 // Native SHP vehicles compose hull and turret at one origin on the temporary
 // surface, then submit one standing sprite. Merge source indices before
 // lighting/xBRZ, so differing frame crops do not create two depth gradients.
 // The vehicle shadow was submitted once before this composition.
 if(art.kind==2&&pose.turretFrame>=0){external::voxel::Image turret;
  if(!decode(art.shape,art.pointer,pose.turretFrame,turret,why)){failure="vehicle-turret/decode:"+art.filename+" frame="+std::to_string(pose.turretFrame)+" "+why;complete=false;}
  else{turret.y+=art.drawY;int left=std::min(out.x,turret.x),top=std::min(out.y,turret.y),right=std::max(out.x+out.width,turret.x+turret.width),bottom=std::max(out.y+out.height,turret.y+turret.height);
   int width=right-left,height=bottom-top;
   if(width<1||height<1||width>resolutionsampling::MaxDimension||height>resolutionsampling::MaxDimension||uint64_t(width)*height>resolutionsampling::MaxPixels){failure="vehicle-turret/combined-bounds";complete=false;}
   else{std::vector<uint32_t> pixels(size_t(width)*height);std::vector<uint8_t> indices(pixels.size());
    auto merge=[&](const external::voxel::Image&part){for(int y=0;y<part.height;++y)for(int x=0;x<part.width;++x){size_t from=size_t(y)*part.width+x;if(!(part.pixels[from]>>24))continue;size_t to=size_t(y+part.y-top)*width+x+part.x-left;pixels[to]=part.pixels[from];if(part.indices.size()==part.pixels.size())indices[to]=part.indices[from];}};
    merge(out);merge(turret);out.x=left;out.y=top;out.width=width;out.height=height;out.pixels=std::move(pixels);out.indices=std::move(indices);out.nativeDepth.composite=true;
   }
  }
 }
 auto component=[&](external::buildings::Part index,int displayed,float depthBias,bool write){const auto&part=art.buildingParts[index];if(!part.pointer||displayed<0)return;
  external::voxel::Image image;if(!decode(part.shape,part.pointer,displayed,image,why)){failure="building-part/decode:"+part.filename+" frame="+std::to_string(displayed)+" "+why;complete=false;return;}
  image.y+=art.drawY;image.objectPalette=true;image.flat=true;image.writeDepth=write;image.depthBias=depthBias;image.nativeDepth.enabled=true;image.nativeDepth.gradient=nativesurface::Gradient::Flat;image.nativeDepth.adjust=-int(depthBias);
  shadow(part.shape,part.pointer,displayed,image,write,2);out.attachments.push_back(std::move(image));
 };
 if(art.kind==3){
  // Native order: body (43D85F), Bib (43D8E9), UnderDoor/UnderRoof
  // (43D9C9/43DA67), Door (43DF3F). All three extras use the ground gradient;
  // only the standing body uses BUILDNGZ. Their own state supplies the frame.
  if(pose.buildingFlags&external::buildings::Constructed)component(external::buildings::Bib,pose.frame,1,true);
  if(unloading){component(roof?external::buildings::UnderRoofDoor:external::buildings::UnderDoor,(pose.buildingFlags&external::buildings::Damaged)?1:0,0,true);component(external::buildings::Door,pose.doorFrame,5,false);}
 }
 if(art.kind==3&&art.hasGuns()&&pose.buildingGunVisible){
  if(!pose.gunPoseValid){failure="building-gun/current-recoil-unavailable";return false;}
  if(voxelRelight_&&presentation_&&!presentationVplFresh_){failure="building-gun/current-vpl-unavailable";return false;}if(!presentation_)vpl();
  const int scale=presentationRasterScale();if(!scale)return false;
  const double yaw=external::visual::radians(external::visual::bucket(pose.rawBody));
  const double barrelPitch=-(int(external::visual::bucket(pose.rawBarrel))-8)*(2*external::visual::Pi/32);
  const unsigned quadrant=(((unsigned(pose.rawBody)>>13)+1)>>1)&3;const bool turretFirst=quadrant==1||quadrant==2;
  for(unsigned n:{turretFirst?0u:1u,turretFirst?1u:0u}){const auto&gun=art.guns[n];if(!gun.geometry)continue;
   int hvaFrame=(n==1&&art.guns[0].geometry)?0:pose.gunFrame%gun.frames;if(hvaFrame<0){failure="building-gun/negative-hva-frame";return false;}
   ActorArt component;component.kind=3;component.type=art.type;component.colors=art.colors;component.voxel=true;component.sharedGeometry=gun.geometry;component.geometryIdentity=gun.identity;component.turretOffset=art.guns[0].geometry?art.gunOffset:0;
   const auto recoil=art.guns[0].geometry?pose.recoil:std::array<float,2>{};external::voxel::Image image;bool drawn=false;
   if(gpuPresentation_)drawn=gpuVoxel(component,yaw,yaw,0,false,scale,nullptr,false,image,0,hvaFrame,barrelPitch,recoil);
   else{std::shared_ptr<const voxel::Model> animated;const auto*model=gun.geometry.get();if(hvaFrame){animated=voxelGeometry_.animated(*model,gun.identity,0,hvaFrame);if(!animated)return false;model=animated.get();}drawn=voxel::render(*model,yaw,art.colors,image,yaw,component.turretOffset,&voxelLight,255,0,false,scale,nullptr,barrelPitch,recoil);}
   if(!drawn){failure="building-gun/raster";return false;}
   image.x+=art.gunX*image.rasterScale;image.y+=(art.gunY+art.drawY)*image.rasterScale;image.objectPalette=true;image.writeDepth=false;image.nativeDepth.enabled=true;image.nativeDepth.voxel=true;image.nativeDepth.gradient=nativesurface::Gradient::Vertical;image.nativeDepth.adjust=art.gunZ+2;
   std::vector<float>().swap(image.depth);out.attachments.push_back(std::move(image));
  }
 }
 if((art.kind==5||art.kind==external::particles::Kind)&&pose.variant>0){out.nativeBlend=uint8_t(unsigned(pose.variant)&3);out.blendSource=std::make_shared<const std::vector<uint32_t>>(out.pixels);unsigned alpha=external::animations::opacity(pose.variant);for(auto&pixel:out.pixels){uint32_t result=0;for(int shift:{0,8,16,24})result|=((((pixel>>shift)&255)*alpha+127)/255)<<shift;pixel=result;}out.intrinsicAlpha=true;}
 out.incomplete=!complete;return true;
}


};
}}

