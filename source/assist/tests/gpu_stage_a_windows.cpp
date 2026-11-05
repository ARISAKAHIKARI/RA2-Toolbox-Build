#ifdef NDEBUG
#undef NDEBUG
#endif
#include "vulkan_scene.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
using namespace external::vulkancompute;
struct Work {const char*name;int width,height,nx,ny;float tileW,tileH;bool filtered=false;int views=1;};
static FrameJob make(const Work&w,int view){
 FrameJob f;f.width=w.width;f.height=w.height;f.nativeScene=true;
 f.basePixels=std::make_shared<const std::vector<uint32_t>>(size_t(w.width)*w.height,0xff345678u);
 f.baseDepth=std::make_shared<const std::vector<float>>(size_t(w.width)*w.height,-10000.f);
 auto pixels=std::make_shared<std::vector<uint32_t>>(60*30);
 for(int y=0;y<30;++y)for(int x=0;x<60;++x)(*pixels)[y*60+x]=(x+y)%19==0?0:0xff000000u|uint32_t((x*7)&255)<<16|uint32_t((y*13)&255)<<8|uint32_t((x*11+y*3)&255);
 for(int y=0;y<w.ny;++y)for(int x=0;x<w.nx;++x){DrawJob d;d.source={pixels,{}, {},60,30};d.left=x*w.tileW+.125f-view*3.25f;d.top=y*w.tileH+.25f-view*1.5f;d.scaleX=w.tileW/60;d.scaleY=w.tileH/30;d.depthBase=float(y*2);if(w.filtered)d.sampling=Sampling::Bilinear;f.draws.push_back(std::move(d));}
 for(int n=0;n<36;++n){auto d=f.draws[n%f.draws.size()];d.left=float((n*97)%(w.width-24));d.top=float((n*53)%(w.height-24));d.scaleX=d.scaleY=.4f;d.depthBase=5000+n;d.translucency=n%4;d.skipZ=n%5==0;d.writeDepth=n%3!=0;f.draws.push_back(std::move(d));}
 return f;
}
static double median(std::vector<double>v){std::sort(v.begin(),v.end());return v[v.size()/2];}
int main(int argc,char**argv){assert(argc==2);const std::string folder=argv[1];FrameResolve gpu;
 const Work cases[]={{"minimap",600,300,100,100,6,3},{"dense_minimap",600,300,180,180,600.f/180,300.f/180},{"zoomout",960,540,34,36,26,14},{"fullres",1920,1080,30,30,60,30},{"filtered_minimap",320,240,80,80,4,3,true},{"multi_viewport",640,360,32,36,20,10,false,3}};
 std::cout<<"{\"passed\":true,\"cases\":[";bool first=true;
 for(const auto&w:cases){std::vector<FrameJob>frames;for(int v=0;v<w.views;++v)frames.push_back(make(w,v));std::vector<double>total,prepare,wait,copy;uint64_t uploaded=0,readbacks=0;std::vector<uint32_t>pixels;
  for(int round=0;round<17;++round){double rt=0,rp=0,rw=0,rc=0;
   for(int view=0;view<w.views;++view){auto&f=frames[view];f.draws.back().left=float((35*97)%(w.width-24))+float(round&1)*.75f;
    if(!gpu.renderFrame(f,pixels)){std::cerr<<"GPU failure "<<w.name<<": "<<gpu.diagnostics().reason<<"\n";return 2;}
    const auto&d=gpu.diagnostics();assert(d.available&&!d.softwareDevice&&pixels.size()==size_t(w.width)*w.height);rt+=d.milliseconds;rp+=d.prepareMilliseconds;rw+=d.waitMilliseconds;rc+=d.readbackMilliseconds;
    if(round>=5){uploaded+=d.uploadedBytes;readbacks+=d.readbacks;}
    if(round==15||round==16){std::string file=folder+"/"+w.name+".view"+std::to_string(view)+(round==15?".odd":"")+".rgba";std::ofstream raw(file,std::ios::binary);raw.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));assert(raw);}
   }if(round>=5){total.push_back(rt);prepare.push_back(rp);wait.push_back(rw);copy.push_back(rc);}
  }
  if(!first)std::cout<<',';first=false;std::cout<<"{\"name\":\""<<w.name<<"\",\"width\":"<<w.width<<",\"height\":"<<w.height<<",\"views\":"<<w.views<<",\"draws_per_view\":"<<frames[0].draws.size()<<",\"device\":\""<<gpu.diagnostics().device<<"\",\"median_total_ms\":"<<median(total)<<",\"median_prepare_ms\":"<<median(prepare)<<",\"median_wait_ms\":"<<median(wait)<<",\"median_readback_ms\":"<<median(copy)<<",\"warm_uploaded_bytes\":"<<uploaded<<",\"warm_readbacks\":"<<readbacks;
#ifdef GPU_ADAPTIVE_BINS
  const auto&cost=gpu.sceneBinCost();std::cout<<",\"bin_size\":"<<cost.size<<",\"references\":"<<cost.references<<",\"pixel_draw_visits\":"<<cost.pixelVisits;
#endif
  std::cout<<'}';
 }std::cout<<"],\"scope\":\"Own hardware FrameResolve; full per-view RGBA readback; no game/window/process access or full pipeline FPS measured\"}\n";
}
