#include "../../external_reader/source/camera_raster.h"
#include <cassert>
#include <iostream>
using namespace external;
int main(){
 camera::Surface s;assert(s.reset(8,8));lighting::State light;voxel::Image back;back.width=back.height=4;back.pixels.assign(16,0xff008000);back.depth.assign(16,0);back.indices.assign(16,20);s.blit(back,0,0,1,1,10,light);
 auto front=back;front.pixels.assign(16,0xff800000);for(int i=0;i<16;++i)front.depth[i]=i%4<2?1:-1;s.blit(front,0,0,1,1,10,light);
 assert(s.color[0]==0xff800000&&s.color[3]==0xff008000); // same sprite contains both sides of the depth boundary
 auto color=s.color;front.depth.assign(16,0);s.blit(front,0,0,1,1,10,light);assert(s.color==color); // ties preserve earlier color
 auto anim=back;anim.pixels.assign(16,0xff000080);anim.writeDepth=false;anim.depth.assign(16,2);s.blit(anim,0,0,1,1,10,light);assert(s.color[0]==0xff000080&&s.depth[0]==11);
 auto glow=back;glow.objectPalette=true;glow.indices.assign(16,245);glow.depth.assign(16,5);light.intensity=0;s.blit(glow,0,0,1,1,10,light);assert(s.color[0]==0xff008000);glow.indices.assign(16,20);s.blit(glow,4,0,1,1,10,light);assert(s.color[4]==0xff000000);
 light.intensity=1000;assert(camera::lit(0xff804020,1,false,light)==0xff804020);light.intensity=800;assert(camera::lit(0xff804020,1,false,light)!=0xff663319); // actual 63-step light ramp
 voxel::Vpl vpl;std::vector<uint8_t> bytes(784+32*256);bytes[8]=32;for(int page=0;page<32;++page)for(int c=0;c<256;++c)bytes[784+page*256+c]=uint8_t((c+page)%256);
 assert(vpl.load(bytes)&&vpl.map(16,5)==21&&vpl.map(255,5)==36);voxel::Model model;voxel::Face face;face.p[0]={0,0,0};face.p[1]={8,0,0};face.p[2]={8,8,0};face.p[3]={0,8,0};face.normal={1,0,0};face.normalIndex=0;face.hasNormal=true;face.color=5;model.faces.push_back(face);
 std::array<uint32_t,256> colors{};for(unsigned i=0;i<256;++i)colors[i]=0xff000000u|i*0x010101;
 voxel::Image facingA,facingB;assert(voxel::render(model,0,colors,facingA,0,0,&vpl,255));assert(voxel::render(model,1.57079632679,colors,facingB,0,0,&vpl,255));
 auto index=[](const voxel::Image&i){for(size_t n=0;n<i.pixels.size();++n)if(i.pixels[n]>>24)return i.indices[n];return uint8_t(0);};assert(index(facingA)!=index(facingB));
 model.faces[0].normalType=4;model.faces[0].normalIndex=1;assert(voxel::render(model,0,colors,facingA,0,0,&vpl,255));
 model.faces[0].normal={0,0,-1};assert(voxel::render(model,0,colors,facingB,0,0,&vpl,255));assert(facingA.pixels==facingB.pixels); // HVA/scale normal cannot change a VPL page
 bytes.resize(800);assert(!vpl.load(bytes));
 // SHP and VXL presentation share premultiplied opacity and indexed lighting.
 voxel::Image preview;preview.width=3;preview.height=1;preview.pixels={0xff804020,0xff804020,0};preview.indices={20,245,0};preview.objectPalette=true;
 auto source=preview.pixels;std::vector<uint32_t> translucent;light.intensity=500;camera::previewPixels(preview,light,124,translucent);
 assert((translucent[0]>>24)==124&&(translucent[1]>>24)==124&&translucent[2]==0&&preview.pixels==source);
 assert((translucent[0]&0xffffff)<(translucent[1]&0xffffff)); // emissive color still bypasses dimming
 for(auto p:translucent)for(int shift:{0,8,16})assert(((p>>shift)&255)<=(p>>24));
 camera::previewPixels(facingA,light,124,translucent);assert(std::any_of(translucent.begin(),translucent.end(),[](uint32_t p){return p>>24==124;}));
 // Voxel raster padding must not move the scene depth anchor. The cache's
 // internal 3D face depth is intentionally absent when composed into the map.
 voxel::Image standing;standing.width=1;standing.height=10;standing.y=-9;standing.depthBottom=-2;standing.pixels.assign(10,0xff804020);standing.writeDepth=false;
 light={};s.reset(8,16);s.depth.assign(s.depth.size(),-14);s.blit(standing,0,10,1,1,0,light,1);
 assert(s.color[1*8]==0xff804020&&s.color[8*8]!=0xff804020); // standing 1-in-3 profile meets a foreground depth edge
 assert(camera::anchorY(3)==-15&&camera::anchorY(5)==0);
 assert(camera::orderKey(2,3,2688,5248)<camera::orderKey(2,5,2560,5120)); // body before zero-offset anim at identical render anchor
 // Equal-height building shadows must not repeatedly halve a pixel. Units
 // remain read-only; a nearer plane still passes the normal depth test.
 voxel::Image shadow;shadow.width=shadow.height=1;shadow.pixels={0xffffffff};shadow.shadow=true;shadow.depth={1};shadow.writeDepth=true;
 s.reset(8,8,0xff808080);s.blit(shadow,0,0,1,1,0,light);assert(s.color[0]==0xff404040&&s.depth[0]==1);
 for(int n=0;n<8;++n)s.blit(shadow,0,0,1,1,0,light);assert(s.color[0]==0xff404040);
 shadow.depth={2};s.blit(shadow,0,0,1,1,0,light);assert(s.color[0]==0xff202020);
 auto body=shadow;body.shadow=false;body.depth={1};body.pixels={0xff808080};body.shadowsAfter=true;body.attachments={shadow};
 s.reset(8,8);s.blit(body,0,0,1,1,0,light);assert(s.color[0]==0xff404040&&s.depth[0]==2); // body then its native shadow plane
 std::array<float,3> n{};assert(voxel::normalVector(4,0,n)&&!voxel::normalVector(4,250,n)&&!voxel::normalVector(0,0,n));
 assert(!s.reset(100000,100000));
 std::cout<<"PASS per-pixel depth, ties, animation read-only depth, emissive indices, quantized light and bounded VPL lookup\n";
}
