#pragma once
namespace featureavailability {
// v1.0.38-pre.1: retain the controls and implementation for later evaluation.
inline constexpr bool SceneRefresh=false,ViewZoom=true;
inline constexpr int refreshMode(int requested){return SceneRefresh&&requested>=0&&requested<=3?requested:0;}
inline constexpr int zoomPercent(int requested){return ViewZoom?requested:100;}
}
