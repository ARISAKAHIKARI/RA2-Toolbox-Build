#pragma once
#include "vulkan_compute.h"
#include <tuple>
namespace external { namespace gpueffects {struct Plan;} namespace gpuvoxel {struct Prepared;} namespace gpushroud {struct Plan;} namespace vulkancompute {
// Immutable aliases must own their actual source Image/ScaledImage storage.
struct Texture {
 std::shared_ptr<const std::vector<uint32_t>> pixels;
 std::shared_ptr<const std::vector<float>> depth;
 std::shared_ptr<const std::vector<uint8_t>> indices;
 int width=0,height=0;
};
enum class Sampling : uint32_t {Nearest=0,Bilinear=1,Adaptive=2,Reconstructed=3};
// Engine instance identity is distinct from shared sprite/pose ownership.
// Attachment paths use exact indices; no hash can alias two components.
struct MotionIdentity {
 uint32_t object=0,id=0,type=0,kind=0;
 std::vector<uint32_t> part;
 bool known=false;
 bool operator<(const MotionIdentity&o)const{return std::tie(object,id,type,kind,part)<std::tie(o.object,o.id,o.type,o.kind,o.part);}
};
struct MotionView {
 std::shared_ptr<const void> context;
 double x=0,y=0,scaleX=1,scaleY=1;
 float depthFar=0,depthNear=0; // stable reversed orthographic scene-Z interval
 uint64_t terrainRevision=0;
 uint32_t tick=0;
 int frame=-1;
};
struct DrawJob {
 std::shared_ptr<const gpuvoxel::Prepared> gpuRaster; // optional deferred GPU mesh raster
 Texture source; // original coverage, nearest original depth/index and dimensions
 std::shared_ptr<const std::vector<uint32_t>> reconstructionSource; // already lit native PARGB for GPU xBRZ
 std::shared_ptr<const std::vector<uint32_t>> reconstructed; // already lit PARGB
 int reconstructedWidth=0,reconstructedHeight=0,reconstructionFactor=0;
 float left=0,top=0,scaleX=1,scaleY=1; // output edges; scale per original raster pixel
 float depthBase=0; // CPU float(base + Image::depthBias), matching Surface order
 int originY=0,depthBottom=0,rasterScale=1; // original Image metadata; bottom resolved
 float lift=1;
 std::array<float,3> gains{{1,1,1}};
 Sampling sampling=Sampling::Nearest;
 bool shadow=false,writeDepth=true,flat=false,objectPalette=false,smoothRGB=false,skipZ=false;
 MotionIdentity motion;
};
struct FrameJob {
 int width=0,height=0;
 std::shared_ptr<const std::vector<uint32_t>> basePixels;
 std::vector<TileJob> baseTiles; // alternative GPU resolve; empty when basePixels exists
 std::shared_ptr<const std::vector<float>> baseDepth; // output Z, or native plane with axis maps below
 int baseDepthWidth=0,baseDepthHeight=0;
 std::shared_ptr<const std::vector<uint32_t>> baseDepthXs,baseDepthYs; // O(w+h) nearest lookup
 std::vector<DrawJob> draws; // attachments already flattened in exact painter order
 std::shared_ptr<const gpushroud::Plan> maskPlan; // optional GPU native-mask construction
 std::shared_ptr<const std::vector<uint8_t>> fogAlpha; // native0..127; optional
 int clipLeft=0,clipTop=0,clipRight=-1,clipBottom=-1; // negative right/bottom = full
 bool collectGuides=false; // no guide buffers, copies or history work when false
 MotionView motion;
 std::shared_ptr<const gpueffects::Plan> effects;
};
}}
