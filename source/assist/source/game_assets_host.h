#include "../../external_reader/source/game_asset_files.h"
#include "../../external_reader/source/asset_decode.h"
namespace gameassets {
static external::assets::Files files;
static void update(){files.update(externalReader,pid,commandMatch());}
static bool cameo(const char*name,std::vector<uint8_t>&out){update();return files.load(name,out);}
static bool read(uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);}
static uint32_t word(uint32_t p){uint32_t v=0;read(p,&v,4);return v;}
static std::string text(uint32_t p,unsigned n){return external::assets::text(read,p,n);}
static std::string theater(int field,int size){unsigned n=word(0x822CF8);return n<6?text(0x7E1B78+n*112+field,size):std::string{};}
static bool palette(bool iso,std::array<uint32_t,256>&out){std::vector<uint8_t> bytes;std::string n=iso?theater(68,10):"UNIT"+theater(78,3);if(n.empty())return false;return files.load(n+".PAL",bytes)&&external::assets::pal(bytes,out);}
static bool sprite(uint32_t type,uint32_t pointer,const std::array<uint32_t,256>&pal,int frame,external::voxel::Image&out){
 std::string filename=external::assets::shapeName(read,pointer);std::vector<uint8_t> bytes;
 if(!filename.empty()&&files.load(filename,bytes)&&external::assets::shp(std::move(bytes),frame,pal,out))return true;
 std::string stem=text(type+0x1FC,25);if(stem.empty())stem=text(type+0x24,24);if(stem.empty())return false;
 // The live SHPReference name above remains authoritative for NewTheater and
 // alternate art. These candidates also cover resources not yet referenced.
 for(auto suffix:{std::string(".SHP"),"."+theater(78,3)})if(files.load(stem+suffix,bytes)&&external::assets::shp(std::move(bytes),frame,pal,out))return true;return false;
}
static bool tile(uint32_t type,unsigned sub,const std::array<uint32_t,256>&pal,external::voxel::Image&out){std::vector<uint8_t> bytes;auto filename=text(type+0x2F5,14);return !filename.empty()&&files.load(filename,bytes)&&external::assets::tmp(std::move(bytes),sub,pal,out);}
static bool actor(uint32_t p,uint32_t type,uint32_t id,double angle,external::voxel::Image&out){
 if(word(p+0x10)!=id)return false;uint32_t owner=word(p+0x21C);std::array<uint32_t,256> pal{};
 if(!external::voxel::palette(read,type,owner,pal)){if(!palette(false,pal))return false;uint8_t rgb[3]{};if(read(owner+22265,rgb,3))for(int i=16;i<32;++i){unsigned f=unsigned(8+i-16);pal[i]=0xff000000u|((unsigned(rgb[0])*f/23)<<16)|((unsigned(rgb[1])*f/23)<<8)|(unsigned(rgb[2])*f/23);}}
 auto stem=text(type+0x1FC,25);if(stem.empty())stem=text(type+0x24,24);std::vector<uint8_t> v,h;external::voxel::Model model;
 double sx=1,sy=1;read(type+0x360,&sx,8);read(type+0x368,&sy,8);if(!std::isfinite(sx)||sx<=0||sx>8)sx=1;if(!std::isfinite(sy)||sy<=0||sy>8)sy=1;
 if(!stem.empty()&&files.load(stem+".VXL",v)&&files.load(stem+".HVA",h)&&external::assets::vxl(v,h,model,0,float(sx),float(sy))){
  int offset=int32_t(word(type+0x720));if(offset<-4096||offset>4096)offset=0;
  for(auto part:{"TUR","BARL"})if(files.load(stem+part+".VXL",v)&&files.load(stem+part+".HVA",h))external::assets::vxl(v,h,model,float(offset)*12/256,float(sx),float(sy));
  return external::voxel::render(model,angle,pal,out)&&word(p+0x10)==id;
 }return sprite(type,word(type+0xA4),pal,0,out)&&word(p+0x10)==id;
}
}
