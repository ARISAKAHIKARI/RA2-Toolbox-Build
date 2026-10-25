#pragma once
#include "repair_visuals.h"
#include "asset_decode.h"
#include <utility>
namespace external { namespace repairvisuals {
// Independent of Context's actor/animation caches: the wrench is one global
// SHP with seven poses, shared by every repairing building in this context.
class Assets {
 uint64_t revision_=~uint64_t(0);uint32_t shape_=0,converter_=0;int tick_=-1;bool checked_=false,paletteReady_=false;
 std::array<uint32_t,256> palette_{};std::array<voxel::Image,7> images_{};std::array<bool,7> ready_{};
public:
 void reset(){revision_=~uint64_t(0);shape_=converter_=0;tick_=-1;checked_=paletteReady_=false;palette_={};images_={};ready_={};}
 template<class Context>const voxel::Image* image(Context&assets,const Sprite&sprite){return imageSource(assets,sprite,0);}
private:
 template<class Context>const voxel::Image* imageSource(Context&,const Sprite&,long){return nullptr;}
 template<class Context>auto imageSource(Context&assets,const Sprite&sprite,int)
  ->decltype(assets.read(uint32_t{},static_cast<void*>(nullptr),size_t{}),assets.files.load("WRENCH.SHP",std::declval<std::vector<uint8_t>&>()),assets.revision(),static_cast<const voxel::Image*>(nullptr)){
  if(sprite.frame<0||sprite.frame>=7)return nullptr;
  auto revision=assets.revision();
  // All buildings share this cursor palette. Recheck at most once per game
  // frame/revision, rather than issuing a 512-byte read for every building.
  if(!checked_||tick_!=sprite.tick||revision!=revision_||shape_!=sprite.shape||converter_!=sprite.converter){
   std::array<uint32_t,256> colors{};paletteReady_=palette(assets.read,sprite.converter,colors);checked_=true;tick_=sprite.tick;
   if(revision!=revision_||shape_!=sprite.shape||converter_!=sprite.converter||(paletteReady_&&colors!=palette_)){
    revision_=revision;shape_=sprite.shape;converter_=sprite.converter;palette_=colors;images_={};ready_={};
   }
  }
  if(!paletteReady_)return nullptr;const auto&colors=palette_;
  auto&out=images_[sprite.frame];if(ready_[sprite.frame])return &out;
  std::string why;voxel::Image candidate;
  bool ok=voxel::sprite(assets.read,sprite.shape,colors,candidate,sprite.frame,&why,255);
  if(!ok){std::vector<uint8_t> bytes;if(assets.files.load("WRENCH.SHP",bytes))ok=voxel::sprite(external::assets::Memory{std::move(bytes)},external::assets::Memory::Base,colors,candidate,sprite.frame,&why,255);}
  if(!ok)return nullptr;
  statusImage(candidate);out=std::move(candidate);ready_[sprite.frame]=true;return &out;
 }
};
}}
