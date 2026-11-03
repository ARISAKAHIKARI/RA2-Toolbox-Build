#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/camera_assets.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/scene_bank.h"
#include "../../external_reader/source/radar_navigation.h"
#include "../source/radar_layout.h"
#include "../source/native_viewport_host.h"
#include <cassert>
#include <cstdio>
#include <iostream>
using namespace external;
// Reuse the existing production Sampler/Renderer test's synthetic map and
// source assets; the runner extracts setup only, never its assertions.
#include MO_SCENE_SETUP
int main(){
 Read read;auto native=setup();publish(native);radar::Sampler<Read>sampler(read);sampler.effects(false,false);radar::Frame data;
 for(int n=0;n<20;++n){assert(sampler.step(1,4096,true,true,true));if(sampler.hasTerrain()){data=sampler.snapshot(100,1);break;}}assert(data.terrain);Assets assets(read);xbrzadapter::gpuPresentation(false);int cases=0,checks=0;size_t stableTotal=0,mismatchTotal=0;
 for(int scale:{50,75,100,125,150}){
  mainview::View v{0x600000,data.terrain->geo.house,0x50000,1,-200,1200,800,600,0,0,632*scale/100,568*scale/100};v.transform={{168,16,632,568},{0,0,v.width,v.height},uint32_t(scale),true};assert(v.valid());const auto crop=mainview::crop(v,data.terrain->geo.offsetX,data.terrain->geo.offsetY);
  radar::SceneRenderer<Assets>target,reference,wrong;target.outputSized(true);reference.outputSized(true);wrong.outputSized(true);
  auto render=[&](auto&r,int w,int h,double cw,double ch){for(int n=0;n<20;++n)if(auto image=r.render(data,w,h,read,assets,{crop.x,crop.y,cw,ch,true},1,false,true,false,true))return image;assert(false);return std::shared_ptr<const radar::Scene>{};};
  auto a=render(target,632,568,crop.width,crop.height),r=render(reference,v.width,v.height,crop.width,crop.height);assert(a->width==632&&a->height==568&&r->width==v.width&&r->height==v.height);++checks;
  size_t stable=0,mismatch=0;for(int y=0;y<568;++y)for(int x=0;x<632;++x){int ix=int(int64_t(x)*v.width/632),iy=int(int64_t(y)*v.height/568);if(ix<2||iy<2||ix+2>=v.width||iy+2>=v.height)continue;auto c=r->pixels[size_t(iy)*v.width+ix];bool same=true;for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)same&=r->pixels[size_t(iy+dy)*v.width+ix+dx]==c;if(!same)continue;++stable;mismatch+=a->pixels[size_t(y)*632+x]!=c;}
  assert(stable>632*568/10);assert(mismatch<=stable/1000);stableTotal+=stable;mismatchTotal+=mismatch;checks+=2;
  auto physicalCrop=render(wrong,632,568,632,568);size_t different=0;for(size_t n=0;n<a->pixels.size();++n)different+=a->pixels[n]!=physicalCrop->pixels[n];if(scale==100)assert(different==0);else assert(different>a->pixels.size()/100);++checks;++cases;
 }
 printf("{\"passed\":true,\"checks\":%d,\"cases\":%d,\"physical_output\":[632,568],\"stable_reference_pixels\":%llu,\"stable_mismatches\":%llu,\"scope\":\"actual Sampler and CPU SceneRenderer R crop to A-sized output; original R-sized render reference and physical-crop counterexample; synthetic terrain/assets\"}\n",checks,cases,(unsigned long long)stableTotal,(unsigned long long)mismatchTotal);
}
