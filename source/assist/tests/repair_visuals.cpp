#include "../../external_reader/source/camera_repair.h"
#include "../../external_reader/source/camera_raster.h"
#include <iostream>
#include <cstring>
// Keep checks and their setup calls active in optimized Windows fixtures.
// Report the failing condition directly in Actions logs.
#define CHECK(condition) do { if (!(condition)) { \
 std::cerr << "FAIL repair_visuals.cpp:" << __LINE__ << ": " << #condition << std::endl; \
 return 1; \
} } while (false)
struct Memory {
 std::vector<uint8_t> bytes=std::vector<uint8_t>(0xC00000);uint32_t blocked=0;int shapeReads=0,paletteReads=0;
 bool operator()(uint32_t p,void*out,size_t n){if(p==blocked||uint64_t(p)+n>bytes.size())return false;if(p==0x60000)++shapeReads;if(p==0x70000)++paletteReads;memcpy(out,bytes.data()+p,n);return true;}
 template<class T>void put(uint32_t p,T value){memcpy(bytes.data()+p,&value,sizeof value);}
};
static std::vector<uint8_t> shape(){
 std::vector<uint8_t> out(8+7*24+7*4);auto put=[&](size_t at,auto value){memcpy(out.data()+at,&value,sizeof value);};
 put(2,uint16_t(4));put(4,uint16_t(6));put(6,uint16_t(7));
 for(int i=0;i<7;++i){size_t f=8+i*24,p=8+7*24+i*4;put(f,uint16_t(1));put(f+2,uint16_t(2));put(f+4,uint16_t(2));put(f+6,uint16_t(2));put(f+20,uint32_t(p));out[p]=uint8_t(i+1);out[p+1]=0;out[p+2]=0;out[p+3]=uint8_t(i+1);}
 return out;
}
struct Context {
 struct Read{Memory*memory;bool operator()(uint32_t p,void*out,size_t n){return (*memory)(p,out,n);}}read;
 explicit Context(Memory&m):read{&m}{}
 struct Files{int calls=0;bool available=false;std::vector<uint8_t> bytes;bool load(const char*,std::vector<uint8_t>&out){++calls;if(!available)return false;out=bytes;return true;}}files;
 uint64_t generation=1;uint64_t revision(){return generation;}
};
int main(){
 using namespace external;Memory m;constexpr uint32_t object=0x10000,convert=0x50000,shp=0x60000;
 auto read=[&](uint32_t p,void*out,size_t n){return m(p,out,n);};repairvisuals::Sprite sample;
 m.put(object,0x7E3EBCu);m.put(object+0x10,42u);m.put(object+0x6E8,uint8_t(1));m.put(object+0x9C,300*256+128);m.put(object+0xA0,200*256+128);m.put(object+0xA4,208);
 m.put(0x89DDC8,shp);m.put(0x87F6C8,convert);m.put(0xA8ED84,100);m.put(0xA8EB60,1);
 CHECK(repairvisuals::sample(read,object,42,sample));CHECK(sample.x==300*256&&sample.y==200*256&&sample.z==208&&sample.frame==0&&sample.tick==100);
 CHECK(!repairvisuals::sample(read,object,43,sample));m.put(object+0x6E8,uint8_t(0));CHECK(!repairvisuals::sample(read,object,42,sample));m.put(object+0x6E8,uint8_t(1));
 m.put(object+0x3CD,uint8_t(1));CHECK(!repairvisuals::sample(read,object,42,sample));m.put(object+0x3CD,uint8_t(0));
 CHECK(!repairvisuals::sample(read,0xFFFFFFF0,42,sample));m.blocked=object+0x9C;CHECK(!repairvisuals::sample(read,object,42,sample));m.blocked=0;
 int idReads=0;auto reused=[&](uint32_t p,void*out,size_t n){bool ok=read(p,out,n);if(p==object+0x10&&++idReads==1)m.put(object+0x10,43u);return ok;};CHECK(!repairvisuals::sample(reused,object,42,sample));m.put(object+0x10,42u);
 for(int speed=0;speed<7;++speed){int period=std::max(2,(112/(speed+1))/4);for(int tick=0;tick<period*3;++tick)CHECK(repairvisuals::animationFrame(tick,speed)==(tick%period)*6/(period-1));}
 CHECK(repairvisuals::animationFrame(-1,1)==-1&&repairvisuals::animationFrame(1,-1)==-1&&repairvisuals::animationFrame(1,7)==-1);
 m.put(convert,0x7E5358u);m.put(convert+4,2);m.put(convert+0x16C,1);m.put(convert+0x174,0x70000u);
 for(int i=1;i<256;++i)m.put(0x70000+i*2,uint16_t(0xF800u|((i&63)<<5)|31));
 auto bytes=shape();memcpy(m.bytes.data()+shp,bytes.data(),bytes.size());Context context{m};context.files.bytes=bytes;
 repairvisuals::Assets assets;CHECK(repairvisuals::sample(read,object,42,sample));sample.frame=3;
 auto image=assets.image(context,sample);CHECK(image&&image->x==-1&&image->y==-1&&image->width==2&&image->height==2&&image->pixels[0]>>24==255&&image->indices[0]==4&&!image->writeDepth&&std::isnan(image->depth[0]));
 int shapes=m.shapeReads,palettes=m.paletteReads;for(int i=0;i<100;++i)CHECK(assets.image(context,sample)==image);CHECK(m.shapeReads==shapes&&m.paletteReads==palettes&&context.files.calls==0);
 // One mouse palette snapshot serves all active repair icons at this tick.
 sample.frame=6;CHECK(assets.image(context,sample)&&m.paletteReads==palettes);
 ++sample.tick;CHECK(assets.image(context,sample)&&m.paletteReads==palettes+1);
 // A cached status image draws above arbitrary world depth, with its own
 // one-shade palette. It neither writes world depth nor inherits dark tint.
 sample.frame=3;image=assets.image(context,sample);camera::Surface canvas;CHECK(canvas.reset(8,8,0xff123456));canvas.depth.assign(64,1e6f);camera::Light neutral(lighting::State{});
 canvas.blit(*image,4,4,1,1,0,{});CHECK(canvas.color[3*8+3]==image->pixels[0]&&canvas.depth[3*8+3]==1e6f);
 // Shared fake asset backends used by renderer fixtures need no new methods.
 struct Fake{} fake;CHECK(!assets.image(fake,sample));
 // A missing loaded SHP reference falls back to the real global filename,
 // while missing assets remain retryable and never cache a successful blank.
 assets.reset();m.put(shp,uint16_t(65535));m.put(shp+12,0u);context.files.available=false;CHECK(!assets.image(context,sample));context.files.available=true;image=assets.image(context,sample);CHECK(image&&context.files.calls>=2);
 // Palette/identity changes invalidate the seven poses without changing
 // the source game SHP or leaking previous colors into the next context.
 uint32_t previous=image->pixels[0];m.put(0x70000+4*2,uint16_t(0x07E0));++sample.tick;image=assets.image(context,sample);CHECK(image&&image->pixels[0]!=previous);
 ++context.generation;int disk=context.files.calls;CHECK(assets.image(context,sample)&&context.files.calls==disk+1);
 m.put(convert+0x16C,53);++sample.tick;CHECK(!assets.image(context,sample));m.put(convert+0x16C,1);
 std::cout<<"PASS native repair flag/identity/center, seven speed-dependent frames, one-shade mouse palette, shared pose cache, memory/disk recovery, fake-backend compatibility and final no-depth status compositing\n";
}
