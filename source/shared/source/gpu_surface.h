#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
namespace gpupresent {
// Published frames retain their GPU storage just as CPU frames retain pixels.
// No engine handles or device API types cross the portable layer contract.
struct Resource {
 int width=0,height=0;uint64_t adapter=0,sequence=0;
 virtual ~Resource()=default;
};
struct Frame {std::shared_ptr<const Resource> resource;};
inline bool valid(const std::shared_ptr<const Frame>&frame,int width,int height){return frame&&frame->resource&&frame->resource->width==width&&frame->resource->height==height;}
// Recovery uses the established CPU-pixel publication path for both backends.
// GPU compute stays enabled; Vulkan/D3D shared-image presentation is suspended.
constexpr bool DirectPresentationEnabled=false;
inline std::atomic<bool>&available(){static std::atomic<bool> value{DirectPresentationEnabled};return value;}
}
