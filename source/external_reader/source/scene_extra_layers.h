#pragma once
#include "radar_map.h"
#include "terrain_sprite.h"
#include "object_canvas.h"
#include "shape_depth.h"
#include "watch_view.h"
#include <tuple>
namespace external { namespace sceneextras {
// GPL-3.0, Vera20K 3d42426: smudge.rs, instances/{bridges,fogged}.rs,
// building_rubble.rs and beacon_art.rs. Cache art, never live object state.
class Layers {
 enum Mode {Overlay,OverlayShadow,Smudge,Rubble,RubbleShadow,FrozenBody,FrozenShadow,Bib,Animation,BeaconShape};
 using Key=std::tuple<uint32_t,uint32_t,int,uint32_t,int,int>;
 struct Cached {std::array<uint32_t,256>palette{};std::shared_ptr<const voxel::Image>image;std::shared_ptr<const std::vector<uint8_t>>rgba;};
 std::map<Key,Cached> images_;size_t bytes_=0;
 using PaletteKey=std::tuple<uint32_t,uint32_t,int,uint32_t>;
 std::map<PaletteKey,std::array<uint32_t,256>>palettes_;
 std::shared_ptr<const radar::Terrain>terrain_;std::vector<size_t>cells_;
 std::map<std::tuple<uint32_t,uint32_t,bool>,int>frameCounts_;
 template<class Assets>int frames(Assets&assets,uint32_t type,uint32_t shape,bool overlay=false){auto key=std::make_tuple(type,shape,overlay);auto i=frameCounts_.find(key);if(i!=frameCounts_.end())return i->second;return frameCounts_[key]=assets.resourceFrames(type,shape,overlay);}
 template<class Read,class Assets>std::shared_ptr<const voxel::Image> sprite(Read read,Assets&assets,uint32_t type,uint32_t shape,int frame,uint32_t owner,Mode mode,int flags=0,uint32_t converter=0){
  if(shape<0x10000||frame<0||frame>=8192)return {};GameView g(read);std::array<uint32_t,256>pal{};
  const bool overlay=mode==Overlay||mode==OverlayShadow;
  PaletteKey paletteKey{type,owner,int(mode),converter};auto cachedPalette=palettes_.find(paletteKey);bool colors=false;
  if(cachedPalette!=palettes_.end()){pal=cachedPalette->second;colors=true;}
  else {if(mode==Smudge)colors=assets.convertPalette(converter,pal);
  else if(mode==Animation)colors=g.template get<uint8_t>(type+0x35C)?assets.spritePalette(type,"",false,owner,pal):assets.particlePalette(pal);
  else if(mode==BeaconShape)colors=assets.debrisPalette(owner,pal);
  else if(mode==Rubble||mode==RubbleShadow||mode==Smudge||(mode>=FrozenBody&&mode<=Bib&&g.template get<uint8_t>(type+0x1702)))colors=assets.palette(true,pal);
  else{auto file=assets::shapeName(read,shape);if(overlay&&file.empty()){auto names=assets.spriteNames(type,shape,true);if(!names.empty())file=names.front();}colors=assets.spritePalette(type,file,overlay,owner,pal);}
  if(!colors&&mode==Smudge)colors=assets.palette(true,pal);if(colors)palettes_[paletteKey]=pal;}if(!colors)return {};
  Key key{type,shape,frame,owner,mode,flags};auto found=images_.find(key);if(found!=images_.end()&&found->second.palette==pal)return found->second.image;
  voxel::Image image;if(!assets.resourceSprite(type,shape,pal,frame,image,mode==Bib||(mode==Rubble&&shape==g.template get<uint32_t>(type+0x14F4)),overlay))return {};
  image.objectPalette=mode==FrozenBody||mode==Animation||mode==Bib;image.writeDepth=true;
  if(mode==Overlay)terrainsprite::overlayDepth(image,g.template get<uint8_t>(type+0x2A9),g.template get<uint8_t>(type+0x2A8),g.template get<uint8_t>(type+0x2B3),g.template get<uint8_t>(type+0x2B5));
  else if(mode==OverlayShadow||mode==RubbleShadow||mode==FrozenShadow){image.shadow=image.flat=true;image.depthBias=0;}
  else if(mode==Smudge||mode==BeaconShape){image.flat=true;image.writeDepth=false;image.skipZ=true;}
  else if(mode==Rubble||mode==Bib)image.flat=true;
  else if(mode==Animation){image.flat=g.template get<uint8_t>(type+0x369);image.writeDepth=false;}
  else if(mode==FrozenBody){image.flat=flags!=0;image.depthBias=flags?0:-float(g.template get<int>(type+0x1520));
   if(!flags){std::array<uint32_t,256>white{};white.fill(0xffffffffu);voxel::Image mask;int fw=1,fh=1;animations::foundation(read,type,fw,fh);
    if(fw<8&&voxel::sprite(read,g.template get<uint32_t>(0x89DDBC),white,mask,0,nullptr,255))camera::buildingDepth(image,mask,fw,fh,g.template get<int>(type+0x1530),g.template get<int>(type+0x1534),true);}}
  if(found!=images_.end()){bytes_-=camera::pixels(*found->second.image)*13;images_.erase(found);}
  size_t cost=camera::pixels(image)*13;if(cost>64*1024*1024)return {};if(images_.size()>=4096||bytes_+cost>64*1024*1024){images_.clear();bytes_=0;}
  auto result=std::make_shared<const voxel::Image>(std::move(image));images_[key]={pal,result};bytes_+=cost;return result;
 }
public:
 std::shared_ptr<const std::vector<uint8_t>>imageBytes(const std::shared_ptr<const voxel::Image>&image){for(auto&item:images_)if(item.second.image==image){if(!item.second.rgba){auto bytes=std::make_shared<std::vector<uint8_t>>(image->pixels.size()*4);std::memcpy(bytes->data(),image->pixels.data(),bytes->size());item.second.rgba=bytes;}return item.second.rgba;}return {};}
 void begin(){palettes_.clear();frameCounts_.clear();}
 void reset(){images_.clear();palettes_.clear();frameCounts_.clear();terrain_.reset();cells_.clear();bytes_=0;}
 template<class Read,class Assets,class Draw>void ground(const radar::Frame&frame,Read read,Assets&assets,const lighting::State&scene,Draw draw){
  if(!frame.terrain)return;const auto&t=*frame.terrain;const auto&geo=t.geo;GameView g(read);lighting::TintCache tints;
  if(terrain_!=frame.terrain){terrain_=frame.terrain;cells_.clear();for(size_t n=0;n<t.art.size();++n)if(t.art[n].cell&&!t.art[n].sourceHidden&&(t.art[n].smudgeType||t.art[n].overlayType))cells_.push_back(n);
  std::stable_sort(cells_.begin(),cells_.end(),[](size_t a,size_t b){auto da=a%512+a/512,db=b%512+b/512;return da!=db?da>db:a%512<b%512;});}
  const auto&cells=cells_;
  // Every occupied smudge cell emits its own material. Partially removed or
  // differently lit footprints must not collapse to a missing origin record.
  for(size_t n:cells){const auto&c=t.art[n];if(!c.smudgeType)continue;int w=g.template get<int>(c.smudgeType+0x298),h=g.template get<int>(c.smudgeType+0x29C);if(w<1||w>32||h<1||h>32||c.smudgeData>=w*h)continue;
   mapbounds::TileOrigin at;if(!geo.tileOrigin(int(n%512),int(n/512),c.level,at))continue;
   auto image=sprite(read,assets,c.smudgeType,g.template get<uint32_t>(c.smudgeType+0xA4),0,0,Smudge,0,g.template get<uint32_t>(c.cell+0x34));if(!image)continue;
   int col=c.smudgeData%w,row=c.smudgeData/w;auto light=lighting::cell(read,c.cell,scene,lighting::Layer::Terrain,true,&tints);
   draw(*image,float(at.x+30+30*(row-col)),float(at.y-15*(row+col)),1,1,0,light,0,0,image);
  }
  // Independent body and shadow sweeps; the second half never uses the
  // body's Latin-square bridge variant. Crate state is always frame zero.
  for(int shadow=0;shadow<2;++shadow)for(size_t n:cells){const auto&c=t.art[n];if(!c.overlayType||(!shadow&&(c.overlay==0xA7||c.overlay==0xB2)))continue;
   uint32_t type=c.overlaySourceType?c.overlaySourceType:c.overlayType,shape=g.template get<uint32_t>(c.overlayType+0xA4);int index=c.overlayFrame;Mode mode=shadow?OverlayShadow:Overlay;
   bool bridge=(c.flags&0x80)!=0,crate=g.template get<uint8_t>(type+0x2AA)!=0,rubble=g.template get<uint8_t>(type+0x2B4)!=0;
   if(crate)index=0;
   if(rubble&&!crate){if(!c.rubbleType||!g.template get<uint8_t>(c.rubbleType+0x1768))continue;type=c.rubbleType;uint32_t custom=g.template get<uint32_t>(type+0x14F4);if(custom){if(shadow)continue;shape=custom;index=0;}else{shape=g.template get<uint32_t>(type+0xA4);int count=frames(assets,type,shape);if(count/2<=3)continue;index=3;}
    mode=shadow?RubbleShadow:Rubble;
   }else if(!shadow&&bridge&&(index==0||index==9)){static constexpr int latin[16]={0,1,2,3,3,2,1,0,2,3,0,1,1,0,3,2};index+=latin[((n/512)&3)*4+((n%512)&3)];}
   if(shadow){int count=frames(assets,type,shape,mode==OverlayShadow);if(count<2||index>=count/2)continue;index+=count/2;}
   auto image=sprite(read,assets,type,shape,index,0,mode);if(!image)continue;
   mapbounds::TileOrigin at;if(!geo.tileOrigin(int(n%512),int(n/512),c.level,at))continue;
   uint32_t source=c.overlaySourceType?c.overlaySourceType:c.overlayType;int offset=watch::overlayOffset(read,c.cell,source,c.overlay,c.overlayFrame);
   float px=float(at.x+30),py=float(at.y+15+offset),base=float((n%512+n/512)*15+offset);if(!shadow&&bridge)base+=60;
   if(shadow&&bridge&&c.overlayFrame>=9&&c.overlayFrame<=17){px-=15;py+=7;}
   auto light=shadow?lighting::State{}:lighting::cell(read,c.cell,scene,bridge?lighting::Layer::Bridge:lighting::Layer::Terrain,true,&tints);
   if(!shadow&&g.template get<uint8_t>(source+0x2A9))light={};else if(!shadow&&g.template get<uint8_t>(source+0x2A8))light=lighting::cell(read,c.cell,scene,lighting::Layer::Object,false);
   draw(*image,px,py,1,1,base,light,2,0,image);
  }
 }
 template<class Read,class Assets,class Draw>void frozen(const radar::Frame&frame,Read read,Assets&assets,const lighting::State&scene,double zscale,Draw draw){
  if(!frame.extras||!frame.terrain||frame.terrain->reveal)return;GameView g(read);const auto&geo=frame.terrain->geo;
  for(const auto&v:frame.extras->frozen){if(v.records.empty())continue;const auto&body=v.records[0];if(g.template get<uint8_t>(body.type+0x1701))continue;
   const float x=float((int64_t(v.point.x)-v.point.y)*30/256+geo.offsetX*30),y=float((int64_t(v.point.x)+v.point.y-256)*15/256-geo.offsetY*15-screenHeight(v.point.z,zscale));
   const float base=float((int64_t(v.point.x)+v.point.y-256)*15/256)-15;
   auto light=lighting::object(read,body.type,v.owner,3,v.point.x,v.point.y,v.point.z,scene);auto top=lighting::at(read,v.point.x,v.point.y,v.point.z,scene);light.intensity=(top.intensity>=0?top.intensity:top.ambient*10)+g.template get<int16_t>(body.type+0x1548);
   uint32_t shape=g.template get<uint32_t>(body.type+0xA4);
   auto emit=[&](uint32_t type,uint32_t shape,int index,Mode mode,int flags,float z,const lighting::State&light){if(auto image=sprite(read,assets,type,shape,index,v.owner,mode,flags))draw(*image,x,y,1,1,z,light,2,0,image);};
   emit(body.type,shape,body.frame,FrozenBody,body.noZShape&255,base,light);
   int count=frames(assets,body.type,shape);if(body.frame<count/2)emit(body.type,shape,body.frame+count/2,FrozenShadow,0,base,{});
   emit(body.type,g.template get<uint32_t>(body.type+0x1518),body.frame,Bib,0,base+1,light);
   for(size_t n=1;n<v.records.size();++n){const auto&r=v.records[n];if(g.template get<int>(r.type+0x2D4)>g.template get<int>(0xA8EB78,2))continue;auto animLight=lighting::at(read,v.point.x,v.point.y,v.point.z,scene);if(g.template get<uint8_t>(r.type+0x35D))animLight={};emit(r.type,g.template get<uint32_t>(r.type+0xA4),r.frame,Animation,0,base-r.zAdjust,animLight);}
  }
 }
 template<class Read,class Assets,class Draw>void beacons(const radar::Frame&frame,Read read,Assets&assets,double zscale,Draw draw){
  if(!frame.extras||!frame.terrain)return;const auto&e=*frame.extras;int half=e.beaconFrames/2;if(half<1||half>4096||frame.frame<0)return;const auto&geo=frame.terrain->geo;
  for(const auto&b:e.beacons){int index=frame.frame%half+(b.selected?half:0);auto image=sprite(read,assets,0,e.beaconShape,index,b.owner,BeaconShape);if(!image)continue;
   float x=float((int64_t(b.point.x)-b.point.y)*30/256+geo.offsetX*30),y=float((int64_t(b.point.x)+b.point.y)*15/256-geo.offsetY*15-screenHeight(b.point.z,zscale));draw(*image,x,y,1,1,0,lighting::State{},0,0,image);}
 }
 template<class Read,class Assets>std::shared_ptr<const voxel::Image> radarBeacon(const radar::Frame&frame,Read read,Assets&assets,uint32_t owner){if(!frame.extras)return {};const auto&e=*frame.extras;if(e.radarFrames<1||e.radarFrames>8192||frame.frame<0)return {};int f=frame.frame%(e.radarFrames*4);return f<e.radarFrames?sprite(read,assets,0,e.radarShape,f,owner,BeaconShape):nullptr;}
};
}}
