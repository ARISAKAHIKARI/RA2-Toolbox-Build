#include "../../shared/source/overlay_composition.h"
#include "../../external_reader/source/main_viewport.h"
#include "../../external_reader/source/native_canvas.h"
#include "../../external_reader/source/radar_shroud_mask.h"
#include <cassert>
#include <climits>
#include <cstring>
#include <cstdio>
#include <map>
using namespace overlaycomposition;
int main(){
 Context c{7,101,22,3,6,4,100,200};
 auto green=std::make_shared<const std::vector<uint32_t>>(6*4,0xff00ff00u);
 auto red=std::make_shared<const std::vector<uint32_t>>(2*2,0xffff0000u);
 Surface scene{3,5,c,{0,0,6,4},true,255,green},hud{1,10,c,{2,1,2,2},true,255,red};
 std::vector<uint32_t> pixels;
 assert(compose(c,{hud,scene},pixels)&&pixels[8]==0xffff0000&&pixels[0]==0xff00ff00);
 scene.order=110;assert(compose(c,{hud,scene},pixels)&&pixels[8]==0xff00ff00);scene.order=5;
 auto previous=c;c.cameraX++;assert(!valid(scene,c)&&!valid(hud,c));
 hud.cameraBound=false;assert(valid(hud,c));assert(compose(c,{scene,hud},pixels)&&pixels[0]==0&&pixels[8]==0xffff0000);
 c=previous;c.token++;assert(!valid(hud,c));c=previous;
 auto stale=hud;stale.context.match++;assert(!valid(stale,c));stale=hud;stale.context.window++;assert(!valid(stale,c));
 stale=hud;stale.context.width++;assert(!valid(stale,c));
 hud.rect={-1,-1,2,2};assert(compose(c,{hud},pixels)&&pixels[0]==0xffff0000&&pixels[1]==0);
 hud.rect={INT_MAX,INT_MAX,2,2};assert(compose(c,{hud},pixels));for(auto p:pixels)assert(p==0);
 hud.rect={INT_MIN,INT_MIN,2,2};assert(compose(c,{hud},pixels));for(auto p:pixels)assert(p==0);
 assert(over(0x80800000,0xff0000ff,255)==0xff80007f);
 assert(over(0xffffffff,0x00000000,128)==0x80808080);
 assert(over(0,0x80800000,255)==0x80800000);
 assert(!compose(Context{1,1,1,1,8192,8192}, {},pixels));
 // Every page boundary and negative-origin crops keep pixel identity at 1:1.
 external::camera::NativeCanvas canvas;assert(canvas.reset(260,133));
 canvas.underlayRect(0,0,260,133,0x112233);canvas.underlayRect(127,0,130,133,0xabcdef);
 // underlay paints untouched pixels only, so use a second independent marker.
 external::voxel::Image image;image.width=3;image.height=2;image.x=image.y=0;image.pixels={0xff102030,0xff203040,0xff304050,0xff405060,0xff506070,0xff607080};image.writeDepth=false;
 canvas.blit(image,127,127,1,1,0,external::lighting::State{});
 for(int y:{-1,0,126,127,128,132})for(int x:{-1,0,126,127,128,259}){
  std::vector<uint32_t> out;assert(canvas.presentRegion(5,4,out,x,y,5,4));
  for(int dy=0;dy<4;++dy)for(int dx=0;dx<5;++dx){bool inside=x+dx>=0&&x+dx<260&&y+dy>=0&&y+dy<133;
   assert(out[dy*5+dx]==(inside?canvas.pixel(x+dx,y+dy):0));}
 }
 assert(canvas.presentRegion(2560,1,pixels,0,0,2560,1));assert(pixels[259]==canvas.pixel(259,0)&&pixels[260]==0);
 assert(!canvas.presentRegion(8192,8192,pixels,0,0,8192,8192));
 external::mainview::View view{0x10000,0x20000,0x30000,50,-100,350,2560,1440,12,20,2300,1400};
 assert(view.valid());auto crop=external::mainview::crop(view,64,32);
 assert(crop.x==-100+64*30&&crop.y==350-32*15&&crop.width==2300&&crop.height==1400);
 auto later=view;later.frame++;assert(external::mainview::sameViewport(view,later));later.cameraX++;assert(!external::mainview::sameViewport(view,later));
 // Test the actual metadata reader, including torn camera/viewport and dialogs.
 std::map<uint32_t,std::vector<uint8_t>> mem;
 auto put=[&](uint32_t at,auto value){mem[at].resize(sizeof value);memcpy(mem[at].data(),&value,sizeof value);};
 put(0xA8B230,view.scenario);put(0xA83D4C,view.house);put(0x887324,view.tactical);put(0xA8ED84,view.frame);put(0xA8E9A0,uint8_t(1));
 put(0xA8E378,uint8_t(1));put(0xA8EDA0,0);put(0xA8ED8C,0);put(0x8A00A4,view.screenW);put(0x8A00A8,view.screenH);
 put(view.tactical+0xB0,view.cameraX);put(view.tactical+0xB4,view.cameraY);
 mem[0x886FA0].resize(16);int rect[]={view.x,view.y,view.width,view.height};memcpy(mem[0x886FA0].data(),rect,16);
 auto read=[&](uint32_t at,void*out,size_t n){
  if(at==view.tactical+0xB0&&n==8){memcpy(out,&view.cameraX,4);memcpy((char*)out+4,&view.cameraY,4);return true;}
  auto i=mem.find(at);if(i==mem.end()||n!=i->second.size())return false;memcpy(out,i->second.data(),n);return true;
 };
 external::mainview::View sampled;assert(external::mainview::sample(read,sampled)&&external::mainview::sameViewport(sampled,view));
 put(0xA8ED8C,1);assert(!external::mainview::sample(read,sampled));put(0xA8ED8C,0);
 auto torn=[&](uint32_t at,void*out,size_t n){bool ok=read(at,out,n);if(at==view.tactical+0xB0&&n==8)((int*)out)[0]++;return ok;};assert(!external::mainview::sample(torn,sampled));
 view.width=INT_MAX;assert(!view.valid());
 // Shroud never turns transparent out-of-backing fallback into opaque black.
 external::radar::shroudmask::Raster mask;mask.alpha={0,64,127};pixels={0,0xff804020,0xff123456};mask.apply(pixels);
 assert(pixels[0]==0&&pixels[2]==0xff123456);
 std::puts("PASS: native viewport/crop, page seams, negative borders, 2560px, premultiplied layers, stable order, epoch/camera rejection and no sidebars");
}
