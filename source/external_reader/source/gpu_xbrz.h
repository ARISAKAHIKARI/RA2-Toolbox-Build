#pragma once
#include <cstdint>
namespace external { namespace gpuxbrz {
// Bind0: readonly native PARGB uint pixels; Bind1: uint corner scratch;
// Bind2: writable reconstructed PARGB uint atlas. Offsets are uint elements.
// Require shaderFloat64. The two dispatches are ordered by a shader-write to
// shader-read memory barrier. Each scale invocation owns one factor² block.
struct Params {uint32_t inputOffset,width,height,outputOffset,factor,flagsOffset;};
static_assert(sizeof(Params)==24,"Vulkan xBRZ push-constant ABI");
constexpr uint32_t GroupSize=8;
inline uint32_t groups(uint32_t size){return (size+GroupSize-1)/GroupSize;}
inline uint64_t flagWords(uint32_t width,uint32_t height){return (uint64_t(width)+1)*(uint64_t(height)+1);}
}}
