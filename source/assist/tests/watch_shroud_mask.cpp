#include "../../external_reader/source/watch_shroud_mask.h"
#include <cassert>
#include <iostream>

int main(){
 using namespace external;
 watch::Frame frame;frame.radius=1;frame.side=3;frame.cx=frame.cy=20;
 frame.shade.assign(9,radar::ShroudFrames{});frame.tiles.assign(9,{});
 frame.tiles[4].level=2;frame.shade[4]={46,4};frame.fogEnabled=true;
 int loads=0;
 auto image=[](int f){voxel::Image out;out.canvasWidth=60;out.canvasHeight=30;out.width=60;out.height=30;out.x=-30;out.y=-15;out.indices.resize(1800);
  for(int y=0;y<30;++y)for(int x=0;x<60;++x)out.indices[y*60+x]=f==46?(x<30?127:2):f==4?64:f==15?2:127;
  return out;
 };
 auto load=[&](bool,int f,voxel::Image&out){++loads;out=image(f);return true;};
 watch::shroudmask::Raster raster;std::vector<uint32_t> pixels(640*512,0xffc08040u);
 // Flat mask starts at x=290,y=256; Level 2 raises the TMP by 30 pixels,
 // but must not move the mask. The asymmetric edge catches a whole-mask shift.
 assert(raster.apply(frame,0,600,640,512,pixels,load));
 assert(pixels[256*640+304]==0xff614120u);
 assert(pixels[256*640+335]==0xff000000u);
 assert(pixels[226*640+304]==0xff030201u);
 assert(loads==2&&raster.builds()==1&&raster.hits()==0);
 pixels.assign(640*512,0xffc08040u);
 assert(raster.apply(frame,0,600,640,512,pixels,load)&&loads==2&&raster.builds()==1&&raster.hits()==1);
 assert(pixels[256*640+304]==0xff614120u); // Reapply to a freshly composed image.
 // Camera movement translates the same native mask with the composed world.
 pixels.assign(640*512,0xffc08040u);
 assert(raster.apply(frame,60,600,640,512,pixels,load));
 assert(pixels[256*640+304]==0xff030201u&&raster.builds()==2);
 // Fog is optional, while unexplored shroud and its edge sprite remain.
 frame.fogEnabled=false;int fogLoads=0;
 pixels.assign(640*512,0xffc08040u);
 assert(raster.apply(frame,0,600,640,512,pixels,[&](bool fog,int f,voxel::Image&out){fogLoads+=fog;out=image(f);return true;}));
 assert(fogLoads==0&&raster.builds()==3&&pixels[256*640+304]==0xffc08040u&&pixels[256*640+335]==0xff030201u);
 // Cell edge changes invalidate the cache even with the same camera.
 frame.shade[4].shroud=-1;pixels.assign(640*512,0xffc08040u);
 assert(raster.apply(frame,0,600,640,512,pixels,load));
 assert(raster.builds()==4&&pixels[256*640+335]==0xffc08040u);
 frame.shade[4].shroud=46;frame.fogEnabled=true;pixels.assign(640*512,0xffc08040u);
 assert(raster.apply(frame,0,600,640,512,pixels,load));
 assert(raster.builds()==5&&loads==3&&pixels[256*640+304]==0xff614120u);
 raster.reset();assert(raster.builds()==0&&raster.hits()==0);
 // A corrupt or unreadable mask cannot publish uncovered terrain.
 watch::shroudmask::Raster missing;pixels.assign(640*512,0xffc08040u);
 assert(!missing.apply(frame,0,600,640,512,pixels,[](bool,int,voxel::Image&){return false;}));
 assert(pixels[256*640+304]==0xffc08040u);
 std::cout<<"PASS watch native flat-cell shroud anchor, transition/fog mask, camera shift and failed asset\n";
}
