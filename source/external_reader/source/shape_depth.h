#pragma once
#include "voxel_preview.h"
namespace external { namespace camera {
inline void buildingDepth(voxel::Image&image,const voxel::Image&mask,int fw,int fh,int moveX,int moveY){
 if(mask.indices.size()!=size_t(mask.width)*mask.height||mask.width<=0||mask.height<=0||fw<1||fh<1||fw>32||fh>32)return;
 image.depth.resize(image.pixels.size());int rawX=mask.x+mask.canvasWidth/2,rawY=mask.y+mask.canvasHeight/2;
 float bottom=float(image.y+image.height-1-14+2);
 for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){
  int mx=image.x+x+198+moveX-(fw-fh)*30-rawX,my=image.y+y+446+moveY-(fw+fh-2)*15-rawY;
  unsigned value=mx>=0&&my>=0&&mx<mask.width&&my<mask.height?mask.indices[size_t(my)*mask.width+mx]:0;
  image.depth[size_t(y)*image.width+x]=bottom+(value?int(value)-66:0);
 }
}
}}
