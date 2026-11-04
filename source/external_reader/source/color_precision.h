#pragma once
#include <algorithm>
#include <cstdint>

// Presentation-only lighting precision. SmoothRGB retains the evaluated
// brightness/tint precision until the final 8-bit channel write. It is not an
// HDR framebuffer and cannot recover RGB already quantized by a RGB565 source.
namespace external { namespace colorprecision {
enum class Mode : uint8_t {NativeSteps, SmoothRGB};
inline float gain(int brightness,int tint,Mode mode=Mode::NativeSteps){
 brightness=std::clamp(brightness,0,2000);tint=std::clamp(tint,0,2000);
 if(mode==Mode::SmoothRGB)return float(brightness)*float(tint)/1000000.f;
 const int shade=std::min(254,(261*brightness)>>11);
 const int level=std::min(62,127*shade*62/32258);
 return level/31.f*tint/1000.f;
}
}}
