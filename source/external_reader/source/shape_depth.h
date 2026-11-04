#pragma once
#include "voxel_preview.h"
namespace external { namespace camera {
inline void buildingDepth(voxel::Image&image,const voxel::Image&mask,int fw,int fh,int moveX,int moveY,bool nativeBiased=false){
 if(mask.indices.size()!=size_t(mask.width)*mask.height||mask.width<=0||mask.height<=0||fw<1||fh<1||fw>32||fh>32)return;
 image.depth.resize(image.pixels.size());image.nativeDepth.shape=image.nativeExtended;image.nativeShape.resize(image.pixels.size());image.nativeShapeCoverage.resize(image.pixels.size());int rawX=mask.x+mask.canvasWidth/2,rawY=mask.y+mask.canvasHeight/2;
 const int shapeTop=rawY-446-moveY+(fw+fh-2)*15-image.y;
 image.nativeDepth.clipTop=std::max(0,shapeTop);image.nativeDepth.clipBottom=std::min(image.height,shapeTop+mask.height);
 float bottom=float(image.y+image.height-1-14+2);
 for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){
  int mx=image.x+x+198+moveX-(fw-fh)*30-rawX,my=image.y+y+446+moveY-(fw+fh-2)*15-rawY;
  unsigned value=mx>=0&&my>=0&&mx<mask.width&&my<mask.height?mask.indices[size_t(my)*mask.width+mx]:0;
  size_t at=size_t(y)*image.width+x;image.nativeShape[at]=image.nativeExtended&&value?-(nativeBiased?int(int8_t(value)):int(int8_t(uint8_t(value-65)))):0;
  image.nativeShapeCoverage[at]=mx>=0&&my>=0&&mx<mask.width&&my<mask.height;
  // Fetch_Z_Data 45E94D subtracts 65 in place. Memory bytes are signed
  // differences already; disk bytes still carry the original unsigned bias.
  image.depth[size_t(y)*image.width+x]=bottom+(value?(nativeBiased?int(int8_t(value))-1:int(value)-66):0);
 }
}
}}
