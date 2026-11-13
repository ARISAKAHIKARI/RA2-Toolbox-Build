#pragma once
#include <cstdint>
#include <mutex>
#include <tuple>
#include "pixel_upgrade.h"
namespace external { namespace presentation {
// The same immutable feature selection accompanies every viewport request.
// Extra reconstruction remains opt-in; supported effects/relighting default on.
struct Profile {
 bool gpu=false,material=false,particles=true,lasers=true,voxelLighting=true,spriteLighting=true,motion=true;
 uint8_t revealUnits=0,overlayShroudOpacity=255;
 int detail=0,precision=0,palette=0,scale=1;
 bool operator==(const Profile&o)const{return std::tie(revealUnits,overlayShroudOpacity,motion,gpu,material,particles,lasers,voxelLighting,spriteLighting,detail,precision,palette,scale)==std::tie(o.revealUnits,o.overlayShroudOpacity,o.motion,o.gpu,o.material,o.particles,o.lasers,o.voxelLighting,o.spriteLighting,o.detail,o.precision,o.palette,o.scale);}
 bool operator!=(const Profile&o)const{return !(*this==o);}
};
struct Selection {Profile profile;uint64_t revision=0;};
class Settings {
 std::mutex mutex_;Selection current_;
public:
 void publish(Profile profile){std::lock_guard<std::mutex>lock(mutex_);if(current_.profile!=profile){current_.profile=profile;++current_.revision;}}
 Selection get(){std::lock_guard<std::mutex>lock(mutex_);return current_;}
};
inline Settings&settings(){static Settings shared;return shared;}
template<class Assets>void apply(const Profile&p,Assets&a,int sourceWidth,int sourceHeight,int width,int height){
 a.presentationDetail(p.detail);a.presentationGPU(p.gpu);a.presentationResolution(sourceWidth,sourceHeight,width,height);
 a.presentationColorPrecision(p.precision!=0);a.presentationPalette(p.palette==1?pixelupgrade::PaletteRange::FullRGB:pixelupgrade::PaletteRange::OriginalDAC);
 a.presentationLighting(p.voxelLighting,p.spriteLighting);
}
}}
