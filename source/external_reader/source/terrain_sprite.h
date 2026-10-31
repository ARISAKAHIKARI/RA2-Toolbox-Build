#pragma once
#include "voxel_preview.h"
// Terrain body/shadow selection and overlay Z policy adapted from Vera20k
// (YuriPlanet and contributors), GPL-3.0; commit 3d42426b2e9c1b4cf913a7dbbaaf8bf7d3b6bc7f,
// src/render/overlay_atlas.rs and src/app/presentation/instances/overlays.rs.
// Changes: C++ host decoder, existing positive scene-depth convention, shared
// CPU/GPU Image attachments. See research/2026-10-30-scene-hotfix18.md.
namespace external {namespace terrainsprite {
template<class Read>bool decode(Read read,uint32_t pointer,const std::array<uint32_t,256>&palette,int frame,
                               bool animated,bool spawner,voxel::Image&out,std::string*reason=nullptr){
 uint8_t head[8]{};
 if(!voxel::range(pointer,16)||!read(pointer,head,8))return false;
 if(voxel::u16(head)==65535){uint32_t loaded=0;if(!read(pointer+12,&loaded,4)||!voxel::range(loaded,8)||!read(loaded,head,8))return false;pointer=loaded;}
 const int frames=voxel::u16(head+6);
 if(!voxel::sprite(read,pointer,palette,out,frame,reason,255))return false;
 out.writeDepth=!animated;out.shadowsAfter=true;
 if(spawner)out.y-=16;
 // Empty body/shadow frames remain empty; never substitute standing frame 0.
 // Cell overlay shadows are emitted separately by scene_extra_layers.h.
 if(frames>=2&&frame>=0&&frame<frames/2){
  voxel::Image shadow;
  if(!voxel::sprite(read,pointer,palette,shadow,frame+frames/2,reason,255))return false;
  if(spawner)shadow.y-=16;
  shadow.shadow=shadow.flat=true;shadow.writeDepth=!animated;
  // SceneRenderer supplies terrain-body lift 12; the flat stencil uses 3.
  shadow.depthBias=-9;out.attachments.push_back(std::move(shadow));
 }
 return true;
}
inline void overlayDepth(voxel::Image&out,bool resource,bool wall,bool drawFlat,bool rock){
 // Resources stay flat even when their declared DrawFlat is false. Veins /
 // chain-reaction flags do not disable depth writes for an opaque body.
 out.flat=resource||(!wall&&drawFlat);
 out.depthBias=out.flat?-1.f:(wall||rock)?0.f:15.f;
 out.writeDepth=true;
}
}}
