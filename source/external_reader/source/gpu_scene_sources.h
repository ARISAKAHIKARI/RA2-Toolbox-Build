#pragma once
#include "gpu_draw_encoding.h"
#include "gpu_voxel.h"
#include "immutable_atlas_plan.h"
namespace external { namespace vulkancompute { namespace scenesources {
using Source=immutableatlas::Source;
using Bounds=std::array<int,4>;
using Indices=std::array<size_t,8>;
static constexpr size_t Missing=size_t(-1);
template<class T>inline Source source(const std::shared_ptr<const std::vector<T>>&v){return v?Source{std::shared_ptr<const void>(v,v.get()),v->data(),v->size()*sizeof(T)}:Source{};}
inline bool active(const Bounds&b){return b[0]<b[2]&&b[1]<b[3];}
inline Bounds clippedBounds(const DrawJob&d,const FrameJob&frame){
 auto b=drawBounds(d,frame.width,frame.height);
 // Non-page scene shaders reject these output pixels before reading sprites,
 // including serial cloak draws. Keep the same integer, half-open clip.
 b[0]=std::max(b[0],std::clamp(frame.clipLeft,0,frame.width));
 b[1]=std::max(b[1],std::clamp(frame.clipTop,0,frame.height));
 b[2]=std::min(b[2],frame.clipRight<0?frame.width:std::clamp(frame.clipRight,0,frame.width));
 b[3]=std::min(b[3],frame.clipBottom<0?frame.height:std::clamp(frame.clipBottom,0,frame.height));
 return b;
}
struct Stats {uint64_t candidatePixels=0,admitted=0,culled=0;bool needs64=false,rawTiles=false,displacement=false;};
// Validate every scene packet, including off-screen packets, before omitting
// its atlas sources. Draw IDs and painter order remain unchanged; an omitted
// packet has empty bounds and Missing offsets and can never enter a bin.
inline const char* append(const FrameJob&frame,std::vector<Source>&colors,std::vector<Source>&materials,std::vector<Indices>&drawSources,std::vector<Bounds>&bounds,Stats&stats){
 stats={};drawSources.clear();bounds.clear();drawSources.reserve(frame.draws.size());bounds.reserve(frame.draws.size());
 constexpr uint64_t MaxPixels=32ull*1024*1024;
 for(const auto&d:frame.draws){
  const auto&t=d.source;
  if((!t.pixels&&!d.gpuRaster)||t.width<1||t.height<1||t.width>8192||t.height>8192||uint64_t(t.width)*t.height>MaxPixels||(t.pixels&&uint64_t(t.width)*t.height!=t.pixels->size())||(t.depth&&t.depth->size()!=uint64_t(t.width)*t.height)||(t.indices&&t.indices->size()!=uint64_t(t.width)*t.height)||d.rasterScale<1||d.rasterScale>4||!std::isfinite(d.left)||!std::isfinite(d.top)||!std::isfinite(d.scaleX)||!std::isfinite(d.scaleY)||d.scaleX<=0||d.scaleY<=0||d.scaleX>8||d.scaleY>8||!std::isfinite(d.depthBase)||!std::isfinite(d.lift)||uint32_t(d.sampling)>3)return "Invalid GPU scene sprite";
  for(float gain:d.gains)if(!std::isfinite(gain)||gain<0||gain>16)return "Invalid GPU scene light";
  if(d.gpuRaster&&(d.gpuRaster->width!=t.width||d.gpuRaster->height!=t.height))return "GPU voxel dimensions disagree";
  if(d.nativeShape&&d.nativeShape->size()!=uint64_t(t.width)*t.height)return "Invalid native Z shape";
  if(d.nativeCoverage&&d.nativeCoverage->size()!=uint64_t(t.width)*t.height)return "Invalid native Z coverage";
  if(d.palette.table&&d.palette.table->size()!=size_t(d.palette.rows)*256)return "Invalid native palette rows";
  if(d.sourcePalette&&(d.sourcePalette->size()!=256||(!d.gpuRaster&&!t.indices)))return "Invalid indexed GPU palette";
  if(d.sampling==Sampling::Reconstructed&&((!d.reconstructed&&!d.reconstructionSource)||(d.reconstructed&&d.reconstructionSource)||d.reconstructionFactor<2||d.reconstructionFactor>4||d.reconstructedWidth!=t.width*d.reconstructionFactor||d.reconstructedHeight!=t.height*d.reconstructionFactor||(d.reconstructed&&d.reconstructed->size()!=uint64_t(d.reconstructedWidth)*d.reconstructedHeight)||(d.reconstructionSource&&d.reconstructionSource->size()!=uint64_t(t.width)*t.height)))return "Invalid reconstructed GPU scene sprite";
  if(d.mixStrength>255||d.translucency>3||!std::isfinite(d.clipBottom)||d.nativeBlend>3||(d.nativeBlend&&(d.sampling!=Sampling::Nearest||d.smoothRGB)))return "Invalid packed GPU blend";
  auto b=clippedBounds(d,frame);bounds.push_back(b);Indices sources;sources.fill(Missing);
  if(!active(b)){drawSources.push_back(sources);++stats.culled;continue;}
  ++stats.admitted;stats.candidatePixels+=uint64_t(b[2]-b[0])*(b[3]-b[1]);stats.needs64|=d.sampling!=Sampling::Nearest;stats.rawTiles|=bool(d.reconstructionSource);stats.displacement|=d.displace;
  sources[0]=colors.size();
  if(d.gpuRaster){auto owner=std::shared_ptr<const void>(d.gpuRaster,d.gpuRaster.get());colors.push_back({owner,nullptr,size_t(t.width)*t.height*4,true,1});sources[2]=materials.size();materials.push_back({owner,nullptr,size_t(t.width)*t.height*4,true,2});}
  else colors.push_back(source(t.pixels));
  if(d.nativeShape){sources[5]=materials.size();materials.push_back(source(d.nativeShape));}
  if(d.nativeCoverage){sources[6]=materials.size();materials.push_back(source(d.nativeCoverage));}
  if(t.depth){sources[1]=materials.size();materials.push_back(source(t.depth));}
  if(t.indices&&!d.gpuRaster){sources[2]=materials.size();materials.push_back(source(t.indices));}
  if(d.palette.table){sources[7]=materials.size();materials.push_back(source(d.palette.table));}
  if(d.sourcePalette){sources[4]=colors.size();colors.push_back(source(d.sourcePalette));}
  if(d.sampling==Sampling::Reconstructed){sources[3]=colors.size();if(d.reconstructionSource){auto owner=std::shared_ptr<const void>(d.reconstructionSource,d.reconstructionSource.get());uint64_t signature=(uint64_t(d.reconstructionFactor)<<56)|(uint64_t(t.width)<<28)|uint64_t(t.height);colors.push_back({owner,nullptr,size_t(d.reconstructedWidth)*d.reconstructedHeight*4,true,signature});}else colors.push_back(source(d.reconstructed));}
  drawSources.push_back(sources);
 }
 return nullptr;
}
}}}
