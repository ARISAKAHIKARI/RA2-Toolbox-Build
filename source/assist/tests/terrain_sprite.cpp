#include "../../external_reader/source/terrain_sprite.h"
#include "../../external_reader/source/asset_decode.h"
#include "../../external_reader/source/camera_raster.h"
#include "../../external_reader/source/gpu_scene_record.h"
#include <cassert>
#include <cstdio>
using namespace external;
int main(){
 std::vector<uint8_t>b(8+4*24+4*4);
 auto half=[&](size_t at,uint16_t v){std::memcpy(b.data()+at,&v,2);};
 auto word=[&](size_t at,uint32_t v){std::memcpy(b.data()+at,&v,4);};
 half(2,20);half(4,20);half(6,4);
 for(int f=0;f<4;++f){const size_t at=8+f*24;half(at,f<2?8:12);half(at+2,f<2?6:12);half(at+4,2);half(at+6,2);word(at+20,8+4*24+f*4);std::fill_n(b.begin()+8+4*24+f*4,4,uint8_t(1+f));}
 std::array<uint32_t,256>pal{};pal.fill(0xff80a060);
 voxel::Image tree;assert(terrainsprite::decode(assets::Memory{b},assets::Memory::Base,pal,0,false,false,tree));
 assert(tree.attachments.size()==1&&tree.shadowsAfter&&tree.writeDepth);
 auto&shadow=tree.attachments[0];assert(shadow.indices[0]==3&&shadow.x==2&&shadow.y==2&&shadow.shadow&&shadow.flat&&shadow.writeDepth);
 camera::Surface canvas;assert(canvas.reset(32,32,0xff808080));canvas.blit(tree,10,10,1,1,50,{},12);
 assert(canvas.color[12*32+12]==0xff404040&&canvas.color[6*32+8]==pal[1]);
 canvas.depth[13*32+13]=10000;canvas.color[13*32+13]=0xffaabbcc;canvas.blit(tree,10,10,1,1,50,{},12);assert(canvas.color[13*32+13]==0xffaabbcc); // foreground survives
 camera::GpuSceneRecord plan;auto colors=std::make_shared<const std::vector<uint32_t>>(32*32,0xff808080);auto depth=std::make_shared<const std::vector<float>>(32*32,-1000);
 plan.begin(32,32,colors,depth,colorprecision::Mode::NativeSteps);auto owner=std::make_shared<const voxel::Image>(tree);assert(plan.append(*owner,owner,10,10,1,1,50,{},12,0,false));
 assert(plan.frame().draws.size()==2&&!plan.frame().draws[0].shadow&&plan.frame().draws[1].shadow&&plan.frame().draws[1].writeDepth);
 voxel::Image mine;assert(terrainsprite::decode(assets::Memory{b},assets::Memory::Base,pal,1,true,true,mine));assert(mine.y==tree.y-16&&!mine.writeDepth&&!mine.attachments[0].writeDepth&&mine.attachments[0].y==shadow.y-16);
 assert(mine.attachments[0].indices[0]==4);
 word(8+3*24+20,0);assert(terrainsprite::decode(assets::Memory{b},assets::Memory::Base,pal,1,false,false,mine));assert(std::all_of(mine.attachments[0].pixels.begin(),mine.attachments[0].pixels.end(),[](auto p){return p==0;}));
 assert(!terrainsprite::decode(assets::Memory{b},assets::Memory::Base,pal,9,false,false,mine));
 voxel::Image ore;terrainsprite::overlayDepth(ore,true,false,false,false);assert(ore.flat&&ore.writeDepth);terrainsprite::overlayDepth(ore,false,true,true,false);assert(!ore.flat&&ore.writeDepth);terrainsprite::overlayDepth(ore,false,false,true,false);assert(ore.flat&&ore.writeDepth);
 std::puts("PASS terrain body/second-half shadow, animated read-only depth, empty frames, spawner offsets, foreground occlusion, CPU pixels and GPU commands without Vulkan execution");
}
