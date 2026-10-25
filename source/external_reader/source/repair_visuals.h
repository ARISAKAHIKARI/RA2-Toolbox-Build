#pragma once
#include "game_view.h"
#include "voxel_preview.h"
namespace external { namespace repairvisuals {
// TechnoClass::DrawExtras 6F5253..6F5345 draws this directly, outside
// AnimClass::Array. This is a screen-status sprite, not building body art.
struct Sprite {
 uint32_t object=0,id=0,shape=0,converter=0;int frame=0,x=0,y=0,z=0,tick=-1;
};
inline int animationFrame(int tick,int gameSpeed){
 if(tick<0||gameSpeed<0||gameSpeed>6)return -1;
 // 5FB2E0 GetAnimSpeed(14), then 6F52E4 /4 with a minimum of two.
 int period=std::max(2,(112/(gameSpeed+1))/4);
 return (tick%period)*6/(period-1);
}
template<class Read>bool sample(Read read,uint32_t object,uint32_t id,Sprite&out){
 if(object<0x10000||object>0xfffff917u||!id)return false;
 uint32_t vtable=0,before=0,after=0,shape=0,converter=0;uint8_t repairing=0,sinking=0;
 int xyz[3]{},tick=-1,speed=-1;
 if(!read(object,&vtable,4)||vtable!=0x7E3EBC||!read(object+0x10,&before,4)||before!=id||
    !read(object+0x6E8,&repairing,1)||!repairing||!read(object+0x3CD,&sinking,1)||sinking||
    !read(object+0x9C,xyz,sizeof xyz)||xyz[0]<0||xyz[1]<0||xyz[0]>=512*256||xyz[1]>=512*256||xyz[2]<0||xyz[2]>100000||
    !read(0x89DDC8,&shape,4)||shape<0x10000||!read(0x87F6C8,&converter,4)||converter<0x10000||
    !read(0xA8ED84,&tick,4)||!read(0xA8EB60,&speed,4))return false;
 int frame=animationFrame(tick,speed);if(frame<0)return false;
 uint8_t repairAfter=0;uint32_t shapeAfter=0,converterAfter=0;
 if(!read(object+0x10,&after,4)||after!=before||!read(object+0x6E8,&repairAfter,1)||repairAfter!=repairing||
    !read(0x89DDC8,&shapeAfter,4)||shapeAfter!=shape||!read(0x87F6C8,&converterAfter,4)||converterAfter!=converter)return false;
 // Building virtual +AC is 459EF0: center = Location - (128,128,0).
 out={object,id,shape,converter,frame,xyz[0]-128,xyz[1]-128,xyz[2],tick};return true;
}
// MOUSE_PAL is a plain one-shade ConvertClass. Its middle RGB565 row is
// already converted; it has no LightConvert UsedPalette1 field at +188.
template<class Read>bool palette(Read read,uint32_t converter,std::array<uint32_t,256>&out){
 if(!voxel::range(converter,0x188))return false;
 uint32_t vtable=0,bpp=0,row=0,again=0;int shades=0;uint16_t colors[256]{};
 if(!read(converter,&vtable,4)||vtable!=0x7E5358||!read(converter+4,&bpp,4)||bpp!=2||
    !read(converter+0x16C,&shades,4)||shades!=1||!read(converter+0x174,&row,4)||!voxel::range(row,sizeof colors)||
    !read(row,colors,sizeof colors)||!read(converter+0x174,&again,4)||again!=row)return false;
 std::array<uint32_t,256> result{};
 for(unsigned i=1;i<256;++i){unsigned c=colors[i],r=(c>>11)&31,g=(c>>5)&63,b=c&31;
  result[i]=0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);
 }out=result;return true;
}
inline void statusImage(voxel::Image&out){
 // Native flags 0xE00 contain no ZRead/ZWrite. Draw after world actors with
 // neutral light; its one-shade cursor palette ignores scene brightness.
 out.objectPalette=false;out.flat=false;out.writeDepth=false;
 out.depth.assign(out.pixels.size(),std::numeric_limits<float>::quiet_NaN());
}
}}
