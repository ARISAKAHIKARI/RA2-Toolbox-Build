#pragma once
#include <cstddef>
#include <cstdint>
namespace external {namespace rendermemory {
// On-demand CPU cache targets for the user's 8 GiB allowance. The native page
// pool and image bank are shared across views, rather than multiplied per view.
// Keep 32-bit compatibility separate. These are not Vulkan device-memory limits.
constexpr bool Wide=sizeof(void*)>=8;
constexpr uint64_t TotalBytes=Wide?8ull*1024*1024*1024:768ull*1024*1024;
constexpr size_t NativePages=Wide?19660:3276; // shared:3GiB / 512MiB RGBA+scene-Z+native-u16-Z
constexpr size_t SharedImages=Wide?768*1024*1024:96*1024*1024;
constexpr size_t SharedTiles=Wide?256*1024*1024:64*1024*1024;
constexpr size_t Geometry=Wide?256*1024*1024:128*1024*1024;
constexpr size_t PosePixels=Wide?32*1024*1024:8*1024*1024;
constexpr size_t LiveFaces=Wide?8*1024*1024:2*1024*1024;
constexpr size_t PacketBytes=Wide?128*1024*1024:64*1024*1024;
constexpr size_t LitPixels=Wide?16*1024*1024:4*1024*1024;
constexpr size_t RasterBytes=Wide?128*1024*1024:64*1024*1024;
constexpr size_t AreaPrefixes=Wide?256*1024*1024:32*1024*1024; // shared CPU area sums
}}
