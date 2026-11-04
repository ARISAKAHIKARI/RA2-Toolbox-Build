#pragma once
#include <cstddef>
#include <cstdint>
namespace external {namespace rendermemory {
// Fixed on-demand CPU cache ceilings. The user's 8 GiB figure is a reference,
// not a global aggregate admission limit. The native page
// pool and image bank are shared across views, rather than multiplied per view.
// Keep 32-bit compatibility separate. These are not Vulkan device-memory limits.
constexpr bool Wide=sizeof(void*)>=8;
constexpr uint64_t TotalBytes=Wide?8ull*1024*1024*1024:768ull*1024*1024;
constexpr size_t NativePages=Wide?35746:2978; // shared:6GiB / 512MiB RGBA+scene-Z+native-u16-Z+A
constexpr size_t SharedImages=Wide?1536*1024*1024:96*1024*1024;
constexpr size_t SharedTiles=Wide?1024*1024*1024:64*1024*1024;
constexpr size_t Geometry=Wide?512*1024*1024:128*1024*1024;
constexpr size_t PosePixels=Wide?128*1024*1024:8*1024*1024;
constexpr size_t LiveFaces=Wide?32*1024*1024:2*1024*1024;
constexpr size_t PacketBytes=Wide?512*1024*1024:64*1024*1024;
constexpr size_t LitPixels=Wide?64*1024*1024:4*1024*1024;
constexpr size_t RasterBytes=Wide?512*1024*1024:64*1024*1024;
constexpr size_t AreaPrefixes=Wide?512*1024*1024:32*1024*1024; // shared CPU area sums
}}
