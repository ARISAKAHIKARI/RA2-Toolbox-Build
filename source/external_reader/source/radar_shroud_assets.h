#pragma once
#include "asset_decode.h"

namespace external { namespace shroudassets {
// Cell::DrawShroud passes frames 0..46 to the 60x30 SHROUD/FOG shapes. The
// SHP bytes are alpha values, not palette colors: 127 is clear, 2 is nearly
// opaque, and 0xFE leaves the destination untouched. Decode through the
// existing SHP reader but retain its original byte indices for composition.
inline bool usable(int frame,const voxel::Image&image){
 if(frame<0||frame>46||image.canvasWidth!=60||image.canvasHeight!=30||image.width<1||image.height<1||
    image.width>60||image.height>30||image.indices.size()!=size_t(image.width)*image.height)return false;
 for(uint8_t value:image.indices)if((value==0||value>127)&&value!=0xFE)return false;
 return true;
}
inline std::array<uint32_t,256> indexPalette(){std::array<uint32_t,256> palette;palette.fill(0xffffffffu);palette[0]=0;return palette;}
template<class Read>bool loaded(Read read,uint32_t pointer,int frame,voxel::Image&out){
 if(pointer<0x10000||frame<0||frame>46)return false;
 voxel::Image image;auto palette=indexPalette();
 if(!voxel::sprite(read,pointer,palette,image,frame,nullptr,255)||!usable(frame,image))return false;
 out=std::move(image);return true;
}
inline bool archived(std::vector<uint8_t> bytes,int frame,voxel::Image&out){
 if(frame<0||frame>46||bytes.empty())return false;
 voxel::Image image;auto palette=indexPalette();
 if(!assets::shp(std::move(bytes),frame,palette,image)||!usable(frame,image))return false;
 out=std::move(image);return true;
}
}}
