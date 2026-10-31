#pragma once
#include "vulkan_scene_types.h"
#include "gpu_effect_plan.h"
#include <optional>
namespace external {namespace camera {
// Compare the completed draw inputs AFTER current state/light/palette/mask
// evaluation. A simulation frame number alone can never justify reuse.
inline bool samePixels(const vulkancompute::TileJob&a,const vulkancompute::TileJob&b){
 return std::tie(a.pixels,a.sourceWidth,a.sourceHeight,a.left,a.top,a.width,a.height,a.sourceX,a.sourceY,a.stepX,a.stepY,a.reconstructionFactor,a.nearest,a.nearestXs,a.nearestYs)==
        std::tie(b.pixels,b.sourceWidth,b.sourceHeight,b.left,b.top,b.width,b.height,b.sourceX,b.sourceY,b.stepX,b.stepY,b.reconstructionFactor,b.nearest,b.nearestXs,b.nearestYs);
}
inline bool samePixels(const vulkancompute::DrawJob&a,const vulkancompute::DrawJob&b){
 if(!(a.nativeDepth==b.nativeDepth)||!(a.nativeWalk==b.nativeWalk)||!(a.palette==b.palette)||std::tie(a.nativeShape,a.nativeCoverage,a.cloakWords,a.nativePitch,a.nativeScaleX,a.nativeScaleY,a.displace)!=std::tie(b.nativeShape,b.nativeCoverage,b.cloakWords,b.nativePitch,b.nativeScaleX,b.nativeScaleY,b.displace))return false;
 return std::tie(a.gpuRaster,a.source.pixels,a.source.depth,a.source.indices,a.source.width,a.source.height,a.reconstructionSource,a.reconstructed,a.reconstructedWidth,a.reconstructedHeight,a.reconstructionFactor,
 a.left,a.top,a.scaleX,a.scaleY,a.depthBase,a.originY,a.depthBottom,a.rasterScale,a.lift,a.gains,a.sampling,a.shadow,a.writeDepth,a.flat,a.objectPalette,a.smoothRGB,a.skipZ,a.sourcePalette,a.nativeBlend,a.translucency,a.clipBottom,a.mixStrength)==
 std::tie(b.gpuRaster,b.source.pixels,b.source.depth,b.source.indices,b.source.width,b.source.height,b.reconstructionSource,b.reconstructed,b.reconstructedWidth,b.reconstructedHeight,b.reconstructionFactor,
 b.left,b.top,b.scaleX,b.scaleY,b.depthBase,b.originY,b.depthBottom,b.rasterScale,b.lift,b.gains,b.sampling,b.shadow,b.writeDepth,b.flat,b.objectPalette,b.smoothRGB,b.skipZ,b.sourcePalette,b.nativeBlend,b.translucency,b.clipBottom,b.mixStrength);
}
inline bool samePixels(const vulkancompute::FrameJob&a,const vulkancompute::FrameJob&b){
 if(std::tie(a.nativeScene,a.nativeScaleX,a.nativeScaleY)!=std::tie(b.nativeScene,b.nativeScaleX,b.nativeScaleY))return false;
 if(a.collectGuides||b.collectGuides||!gpueffects::same(a.effects,b.effects))return false; // temporal guides carry history, not just pixels
 if(std::tie(a.width,a.height,a.basePixels,a.baseDepth,a.baseDepthWidth,a.baseDepthHeight,a.baseDepthXs,a.baseDepthYs,a.maskPlan,a.fogAlpha,a.clipLeft,a.clipTop,a.clipRight,a.clipBottom)!=
    std::tie(b.width,b.height,b.basePixels,b.baseDepth,b.baseDepthWidth,b.baseDepthHeight,b.baseDepthXs,b.baseDepthYs,b.maskPlan,b.fogAlpha,b.clipLeft,b.clipTop,b.clipRight,b.clipBottom)||a.baseTiles.size()!=b.baseTiles.size()||a.draws.size()!=b.draws.size())return false;
 for(size_t n=0;n<a.baseTiles.size();++n)if(!samePixels(a.baseTiles[n],b.baseTiles[n]))return false;
 for(size_t n=0;n<a.draws.size();++n)if(!samePixels(a.draws[n],b.draws[n]))return false;
 return true;
}
template<class Result>class FrameReuse {
 std::optional<vulkancompute::FrameJob> previous_;
 std::shared_ptr<const Result> result_;
public:
 void reset(){previous_.reset();result_.reset();}
 std::shared_ptr<const Result> get(const vulkancompute::FrameJob&frame)const{return previous_&&result_&&samePixels(*previous_,frame)?result_:nullptr;}
 void completed(const vulkancompute::FrameJob&frame,std::shared_ptr<const Result>result){if(!result||frame.collectGuides){reset();return;}previous_=frame;result_=std::move(result);}
};
}}
