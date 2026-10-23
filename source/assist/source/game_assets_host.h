#include "../../external_reader/source/game_asset_files.h"
#include "../../external_reader/source/asset_decode.h"
#include "../../external_reader/source/visual_pose.h"
namespace gameassets {
static external::assets::Files files;
static std::string failure;
static void update(){files.update(externalReader,pid,commandMatch());}
static bool cameo(const char*name,std::vector<uint8_t>&out){update();return files.load(name,out);}
static bool read(uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}
static uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
static std::string text(uint32_t p,unsigned n){return external::assets::text(read,p,n);}
static std::string theater(int field,int size){unsigned n=word(0x822CF8);return n<6?text(0x7E1B78+n*112+field,size):std::string{};}
static bool palette(bool iso,std::array<uint32_t,256>&out){std::vector<uint8_t> bytes;std::string n=iso?theater(68,10):"UNIT"+theater(78,3);if(n.empty()){failure="palette-theater-name";return false;}n+=".PAL";if(!files.load(n,bytes)){failure="palette-not-found:"+n;return false;}if(!external::assets::pal(bytes,out)){failure="palette-format:"+n;return false;}return true;}
static bool sprite(uint32_t type,uint32_t pointer,const std::array<uint32_t,256>&pal,int frame,external::voxel::Image&out){
 std::string filename=external::assets::shapeName(read,pointer);std::vector<uint8_t> bytes;
 if(!filename.empty()&&files.load(filename,bytes)&&external::assets::shp(std::move(bytes),frame,pal,out))return true;
 std::string stem=text(type+0x1FC,25);if(stem.empty())stem=text(type+0x24,24);if(stem.empty())return false;
 // The live SHPReference name above remains authoritative for NewTheater and
 // alternate art. These candidates also cover resources not yet referenced.
 for(auto suffix:{std::string(".SHP"),"."+theater(78,3)})if(files.load(stem+suffix,bytes)&&external::assets::shp(std::move(bytes),frame,pal,out))return true;failure="sprite-load/decode:"+(filename.empty()?stem:filename)+" frame="+std::to_string(frame);return false;
}
static bool tile(uint32_t type,unsigned sub,const std::array<uint32_t,256>&pal,external::voxel::Image&out){std::vector<uint8_t> bytes;auto filename=text(type+0x2F5,14);if(filename.empty()){failure="tile-name-empty type="+std::to_string(type);return false;}if(!files.load(filename,bytes)){failure="tile-not-found:"+filename;return false;}if(!external::assets::tmp(std::move(bytes),sub,pal,out)){failure="tile-format:"+filename+" sub="+std::to_string(sub);return false;}return true;}
static bool actor(uint32_t p,uint32_t type,uint32_t id,double angle,external::voxel::Image&out){
 if(word(p+0x10)!=id)return false;uint32_t owner=word(p+0x21C);std::array<uint32_t,256> pal{};
 if(!external::voxel::palette(read,type,owner,pal)){if(!palette(false,pal))return false;uint8_t rgb[3]{};if(read(owner+22265,rgb,3))for(int i=16;i<32;++i){unsigned f=unsigned(8+i-16);pal[i]=0xff000000u|((unsigned(rgb[0])*f/23)<<16)|((unsigned(rgb[1])*f/23)<<8)|(unsigned(rgb[2])*f/23);}}
 auto stem=text(type+0x1FC,25);if(stem.empty())stem=text(type+0x24,24);std::vector<uint8_t> v,h;external::voxel::Model model;
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 if(!stem.empty()&&files.load(stem+".VXL",v)&&files.load(stem+".HVA",h)&&external::assets::vxl(v,h,model,0,float(sx),float(sy))){
  int offset=int32_t(word(type+0x720));if(offset<-4096||offset>4096)offset=0;
  for(auto part:{"TUR","BARL"})if(files.load(stem+part+".VXL",v)&&files.load(stem+part+".HVA",h))external::assets::vxl(v,h,model,float(offset)*12/256,float(sx),float(sy));
  return external::voxel::render(model,angle,pal,out)&&word(p+0x10)==id;
 }external::GameView game(&read);int heading=int(std::lround(angle*32/(2*external::visual::Pi)))&31;
 return sprite(type,word(type+0xA4),pal,external::visual::previewFrame(game,type,game.kind(p),heading),out)&&word(p+0x10)==id;
}
// Observation assets are retained separately from rendered headings: turning a
// unit must not reopen/redecode its VXL/HVA files every overlay frame.
struct ActorArt {
 external::voxel::Model model;std::vector<uint8_t> shape;std::array<uint32_t,256> colors{};
 uint32_t pointer=0;float turretOffset=0;bool voxel=false;
};
static bool captureActor(uint32_t p,uint32_t type,uint32_t id,ActorArt&out){
 failure.clear();if(word(p+0x10)!=id){failure="actor-identity";return false;}ActorArt next;uint32_t owner=word(p+0x21C);
 if(!external::voxel::palette(read,type,owner,next.colors)){
  if(!palette(false,next.colors))return false;uint8_t rgb[3]{};
  if(read(owner+22265,rgb,3))for(int i=16;i<32;++i){unsigned f=unsigned(8+i-16);next.colors[i]=0xff000000u|((unsigned(rgb[0])*f/23)<<16)|((unsigned(rgb[1])*f/23)<<8)|(unsigned(rgb[2])*f/23);}
 }
 auto stem=text(type+0x1FC,25);if(stem.empty())stem=text(type+0x24,24);
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 auto part=[&](const std::string&suffix,uint32_t at,bool turret){
  external::voxel::Model model;std::vector<uint8_t> v,h;
  bool ok=!stem.empty()&&files.load(stem+suffix+".VXL",v)&&files.load(stem+suffix+".HVA",h)&&external::assets::vxl(v,h,model,0,float(sx),float(sy));
  if(!ok)ok=external::voxel::capturePart(read,word(at),word(at+4),model,0,float(sx),float(sy));
  if(ok){for(auto&f:model.faces)f.turret=turret;next.model.faces.insert(next.model.faces.end(),model.faces.begin(),model.faces.end());}return ok;
 };
 if(part("",type+0xB0,false)){
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
  next.pointer=word(type+0xA4);std::string filename=external::assets::shapeName(read,next.pointer);
  bool loaded=!filename.empty()&&files.load(filename,next.shape);
  if(!loaded&&!stem.empty())for(auto suffix:{std::string(".SHP"),"."+theater(78,3)})if(files.load(stem+suffix,next.shape)){loaded=true;break;}
  if(!loaded){external::voxel::Image check;if(!external::voxel::sprite(read,next.pointer,next.colors,check)){failure="sprite-not-found/unloaded:"+(filename.empty()?stem:filename);return false;}}
 }
 if(word(p+0x10)!=id||word(p+0x21C)!=owner)return false;out=std::move(next);return true;
}
static bool renderActor(const ActorArt&art,const external::visual::Pose&pose,external::voxel::Image&out){
 if(art.voxel)return external::voxel::render(art.model,external::visual::radians(pose.body),art.colors,out,external::visual::radians(pose.turret),art.turretOffset);
 if(pose.frame<0){failure="sprite-sequence";return false;}
 bool ok=false;
 if(!art.shape.empty()){auto bytes=[&](uint32_t at,void*p,size_t n){if(at<external::assets::Memory::Base||uint64_t(at-external::assets::Memory::Base)+n>art.shape.size())return false;memcpy(p,art.shape.data()+at-external::assets::Memory::Base,n);return true;};ok=external::voxel::sprite(bytes,external::assets::Memory::Base,art.colors,out,pose.frame);}
 else ok=external::voxel::sprite(read,art.pointer,art.colors,out,pose.frame);
 if(!ok)failure="sprite-frame/decode:"+std::to_string(pose.frame);return ok;
}

}
