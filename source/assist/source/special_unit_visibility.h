#pragma once
#include "../../external_reader/source/radar_live_shroud.h"
#include "../../external_reader/source/projection.h"
namespace specialoverlay {
// Apply native destination-pixel visibility after composing the real unit.
// Neither disguise removal nor the overlay itself grants map visibility.
class Visibility {
 external::radar::shroudmask::Live live_;
 external::radar::shroudmask::Snapshot prior_;
 external::radar::shroudmask::Raster mask_;
 std::shared_ptr<const external::voxel::Image> input_,output_;
 double x_=0,y_=0;unsigned opacity_=255;
public:
 void reset(){live_.reset();prior_={};mask_={};input_.reset();output_.reset();}
 template<class Read,class Assets>bool apply(Read read,Assets&assets,const external::Projection&p,unsigned opacity,
                                           std::shared_ptr<const external::voxel::Image>&image){
  if(!image||!opacity)return true;
  if(opacity>255||image->width<1||image->height<1||image->pixels.size()!=size_t(image->width)*image->height)return false;
  using namespace external;using namespace radar;Geometry g;bool valid=true;
  auto get=[&](uint32_t address,auto&value){valid=read(address,&value,sizeof value)&&valid;};
  g.bounds=mapbounds::sample(read);g.width=g.bounds.width;g.height=g.bounds.height;
  get(Map+0x13C,g.cells);get(Map+0x140,g.count);get(Map+0x1488,g.factor);
  get(Map+0x14A4,g.rw);get(Map+0x14A8,g.rh);get(Map+0x1240,g.rawWidth);get(Map+0x1244,g.rawHeight);
  get(Map+0x1490,g.offsetX);get(Map+0x1498,g.offsetY);get(0xA83D4C,g.house);
  if(!valid||!g.valid())return false;
  const double x=image->x+double(p.cameraX)+g.offsetX*30.,y=image->y+double(p.cameraY)-g.offsetY*15.;
  shroudmask::Snapshot current;
  if(!live_.capture(read,g,x,y,image->width,image->height,current))return false;
  const bool same=x==x_&&y==y_&&mask_.width==image->width&&mask_.height==image->height&&prior_.sameMask(current);
  if(same&&opacity==opacity_&&image==input_&&output_){image=output_;return true;}
  if(!same&&!mask_.build(g,current.cells,image->width,image->height,image->width,image->height,current.fog,
    [&](bool fog,int frame,voxel::Image&out){return assets.shroud(fog,frame,out);},x,y))return false;
  auto next=std::make_shared<voxel::Image>(*image);
  for(size_t n=0;n<next->pixels.size();++n){
   const unsigned native=std::min(127u,unsigned(mask_.alpha[n]));
   // Native black uses 2 rather than 0. A fully covered overlay pixel must
   // disappear, allowing the game's own black shroud to remain untouched.
   if(opacity==255&&native<=2){next->pixels[n]=0;continue;}
   const unsigned visible=127-((127-native)*opacity)/255;
   const uint32_t pixel=next->pixels[n];uint32_t shaded=pixel&0xff000000u;
   for(int shift:{0,8,16})shaded|=((((pixel>>shift)&255u)*visible+63)/127)<<shift;
   next->pixels[n]=shaded;
  }
  input_=image;output_=next;image=std::move(next);prior_=std::move(current);x_=x;y_=y;opacity_=opacity;return true;
 }
};
}
