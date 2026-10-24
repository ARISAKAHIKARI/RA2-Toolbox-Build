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
 bytes.resize(800);assert(!vpl.load(bytes));
 std::array<float,3> n{};assert(voxel::normalVector(4,0,n)&&!voxel::normalVector(4,250,n)&&!voxel::normalVector(0,0,n));
 assert(!s.reset(100000,100000));
 std::cout<<"PASS per-pixel depth, ties, animation read-only depth, emissive indices, quantized light and bounded VPL lookup\n";
}
