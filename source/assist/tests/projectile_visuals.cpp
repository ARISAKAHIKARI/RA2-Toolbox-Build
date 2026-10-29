#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
#include "../../external_reader/source/projectile_visuals.h"
#include "../../external_reader/source/voxel_preview.h"
#include "../../external_reader/source/camera_raster.h"
int main(){
 std::vector<uint8_t> memory(0xC00000);uint32_t fail=0,race=0;int raceReads=0;
 auto read=[&](uint32_t p,void*out,size_t n){if(p==fail||uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);if(p==race&&++raceReads>1&&n==4){uint32_t bad=999;memcpy(out,&bad,4);}return true;};
 auto put=[&](uint32_t p,const auto&v){memcpy(memory.data()+p,&v,sizeof v);};
 constexpr uint32_t p=0x10000,t=0x20000,convert=0x30000,scheme=0x40000,items=0x50000;
 put(p,external::projectiles::Vtable);put(p+0x10,uint32_t(42));put(p+0xAC,t);put(t,external::projectiles::TypeVtable);memory[p+0x90]=1;put(p+0x94,2);
 put(p+0x9C,2560);put(p+0xA0,2816);put(p+0xA4,416);put(external::projectiles::Array+4,items);put(external::projectiles::Array+8,1);put(external::projectiles::Array+16,1);put(items,p);
 put(0x87F6C4,convert);put(convert,uint32_t(0x7E5358));memory[t+0x2A1]=1;
 external::projectiles::State s;assert(external::projectiles::capture(read,p,s));assert(s.id==42&&s.pose.frame==0&&s.drawZ==-30&&s.converter==convert&&s.pose.variant==int(convert));
 // +6C is damage, including healing/zero-damage visual projectiles.
 for(int damage:{-200,0,1,500}){put(p+0x6C,damage);assert(external::projectiles::capture(read,p,s));}
 assert(external::projectiles::all(read).size()==1);assert(s.layer==3);memory[t+0x2F7]=1;assert(external::projectiles::capture(read,p,s)&&s.layer==1);memory[t+0x2F7]=0;
 memory[t+0x2A1]=0;double velocity[3]{1,0,0};memcpy(memory.data()+p+0xE8,velocity,sizeof velocity);
 for(auto test:std::vector<std::array<double,3>>{{1,0,20},{0,1,12},{-1,0,4},{0,-1,28}}){velocity[0]=test[0];velocity[1]=test[1];memcpy(memory.data()+p+0xE8,velocity,sizeof velocity);assert(external::projectiles::capture(read,p,s)&&s.pose.frame==int(test[2])&&s.pose.sequence==0&&s.pose.body==((28-int(test[2]))&31));}
 memory[t+0x2F4]=2;memory[p+0x12C]=17;assert(external::projectiles::capture(read,p,s)&&s.pose.frame==17&&s.pose.sequence==1&&s.pose.body==0);memory[t+0x2F4]=0;memory[t+0x2F5]=2;assert(external::projectiles::capture(read,p,s)&&s.pose.frame==17&&s.pose.sequence==1);memory[t+0x2F5]=0;
 for(uint32_t address:{p+0x10,p+0xAC,p+0x81,p+0x90,p+0x9C,p+0x158,t+0x29E,t+0x236,t+0x2A1,t+0x2F4,t+0x2A8}){fail=address;external::projectiles::State old;old.id=777;assert(!external::projectiles::capture(read,p,old)&&old.id==777);}fail=0;
 for(uint32_t address:{p+0x81,p+0x158,t+0x29E}){memory[address]=1;assert(!external::projectiles::capture(read,p,s));memory[address]=0;}
 memory[p+0x90]=0;assert(!external::projectiles::capture(read,p,s));memory[p+0x90]=1;
 race=p+0x10;raceReads=0;assert(!external::projectiles::capture(read,p,s));race=0;
 velocity[0]=std::numeric_limits<double>::quiet_NaN();memcpy(memory.data()+p+0xE8,velocity,sizeof velocity);assert(!external::projectiles::capture(read,p,s));velocity[0]=1;velocity[1]=0;memcpy(memory.data()+p+0xE8,velocity,sizeof velocity);
 // FirersPalette uses the shot's inherited index, not a later firer owner.
 memory[t+0x2A9]=1;put(p+0x114,0);put(0xB054D4,items+0x100);put(0xB054E0,1);put(items+0x100,scheme);put(scheme+0x30C,convert);
 assert(external::projectiles::capture(read,p,s)&&s.palette==external::projectiles::SchemePalette);
 put(p+0x114,-1);put(0xA83D4C,uint32_t(0x60000));put(0x60000+0x16054,0);assert(external::projectiles::capture(read,p,s));
 memory[t+0x2A8]=1;put(0x87F6C0,convert+0x1000);assert(external::projectiles::capture(read,p,s)&&s.converter==convert+0x1000&&s.palette==external::projectiles::AnimationPalette);
 memory[t+0x236]=1;put(t+0x2D4,0);velocity[2]=1;memcpy(memory.data()+p+0xE8,velocity,sizeof velocity);assert(external::projectiles::capture(read,p,s)&&s.voxel&&s.pose.body==8&&s.pose.turret==4&&s.pose.frame==0&&s.drawZ==0&&s.palette==external::projectiles::SchemePalette);
 auto light=external::projectiles::light(read,s);assert(light.intensity==1000&&(light.tint==std::array<int,3>{1000,1000,1000}));
 put(convert,uint32_t(0x7ED0A4));put(convert+0x198,750);put(convert+0x19C,800);put(convert+0x1A0,1200);assert((external::projectiles::light(read,s).tint==std::array<int,3>{750,800,1200}));
 struct NoAssets{}none;int art=0;assert(!external::projectiles::captureArt(none,p,t,42,art));struct Assets{bool captureProjectile(uint32_t,uint32_t,uint32_t,int&out){out=19;return true;}}assets;assert(external::projectiles::captureArt(assets,p,t,42,art)&&art==19);
 // Dedicated projectile page selection omits the vehicle specular term.
 external::voxel::VoxelLight numeric;numeric.source={1,0,0};numeric.halfway={1,0,0};assert(external::voxel::nativeProjectilePage({1,0,0},numeric)==16);assert(external::voxel::nativeVoxelPage({1,0,0},numeric)==32);assert(external::voxel::nativeProjectilePage({-1,0,0},numeric)==0);
 // Same-frame shadows appear only above ground; ordinary attachment behavior
 // is unchanged. This is a compositor fixture, not original-game output.
 external::voxel::Image image;image.width=image.height=1;image.pixels={0xffffffff};image.depthBias=100;image.writeDepth=false;
 auto shadow=image;shadow.shadow=true;shadow.flat=true;shadow.airborneShadow=true;shadow.depthBias=100;image.attachments.push_back(shadow);
 external::camera::Surface ground,air;ground.reset(8,8);air.reset(8,8);std::fill(ground.color.begin(),ground.color.end(),0xff808080);std::fill(air.color.begin(),air.color.end(),0xff808080);
 ground.blit(image,2,2,1,1,0,{});air.blit(image,2,2,1,1,0,{},1,2);assert(ground.color[4*8+2]==0xff808080);assert(air.color[4*8+2]==0xff404040);
 std::cout<<"PASS projectile inventory, zero/negative damage, native sprite directions/animated override, VXL yaw/pitch, palette modes, read/race rejection, diffuse-only VPL and airborne shadow composition\n";
}
