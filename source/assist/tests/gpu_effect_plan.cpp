#include "../../external_reader/source/gpu_effect_plan.h"
#include "../../shared/source/overlay_composition.h"
#include <cassert>
#include <iostream>
using namespace external;
// CPU-only admission/geometry oracle. This test never creates a graphics
// device; shader execution and Windows composition require a real-game run.
static int hits(const gpueffects::Command&c,int x,int y){auto&w=c.words;int ax=int(w[8]),ay=int(w[9]),bx=int(w[10]),by=int(w[11]),dx=std::abs(bx-ax),dy=std::abs(by-ay),sx=ax<bx?1:-1,sy=ay<by?1:-1,r=int(w[13]);bool mx=dx>=dy;int major=mx?dx:dy,minor=mx?dy:dx;if(!major)return std::abs(x-ax)<=r&&std::abs(y-ay)<=r;int center=(mx?x-ax:y-ay)*(mx?sx:sy),out=0;for(int n=std::max(0,center-r);n<=std::min(major,center+r);++n){int m=(n*minor*2+major)/(major*2),xx=ax+(mx?n:m)*sx,yy=ay+(mx?m:n)*sy;if(std::abs(x-xx)<=r&&std::abs(y-yy)<=r)++out;}return out;}
int main(){
 worldfx::View v;v.width=v.height=8;v.pixelsW=v.pixelsH=8;
 size_t cases=0;
 for(int ay=0;ay<8;++ay)for(int ax=0;ax<8;++ax)for(int by=0;by<8;++by)for(int bx=0;bx<8;++bx)for(int thick:{1,3,5}){
  nativefeedback::Canvas cpu;assert(cpu.reset(8,8));cpu.line(ax,ay,bx,by,0x280a1425u,thick);
  gpueffects::Builder b(v);b.line(ax,ay,bx,by,0x280a1425u,thick);auto p=b.finish();assert(p->valid&&p->commands.size()==1);
  for(int y=0;y<8;++y)for(int x=0;x<8;++x){uint32_t c=0;for(int i=0,n=hits(p->commands[0],x,y);i<n;++i)c=nativefeedback::over(0x280a1425u,c);assert(c==cpu.pixels[size_t(y)*8+x]);}++cases;
 }
 v.width=v.height=128;v.pixelsW=v.pixelsH=128;
 gpueffects::Builder b(v);b.pixel(2,3,0,1,false);b.line({-100,5},{10,5},0xff0000);
 worldfx::Wave wave;wave.from={0,0,0};wave.to={512,0,0};wave.corners={{{0,0,0},{0,-128,0},{512,0,0},{512,-128,0}}};wave.front=.1;wave.back=.9;wave.kind=3;wave.direction=2;b.wave(wave);b.pixel(8,8,0xff);
 auto p=b.finish();assert(p->valid&&p->commands.size()==4&&p->batches.size()==3);assert(!p->batches[0].snapshot&&p->batches[1].snapshot&&!p->batches[2].snapshot);assert(p->commands[0].words[1]==0&&p->commands[0].words[12]==0);
 std::vector<uint32_t>words={123,456},alphas(p->commands.size(),0xffffffffu);std::vector<gpueffects::Dispatch>jobs;assert(gpueffects::pack(*p,alphas,17,words,jobs)&&jobs.size()==3&&words[0]==123);
 for(size_t n=0;n<jobs.size();++n){auto&d=jobs[n];assert(d.params[6]==17&&d.params[2]==2&&d.snapshot==p->batches[n].snapshot);for(unsigned bin=0;bin<16;++bin){uint32_t start=words[d.params[3]+bin*2],count=words[d.params[3]+bin*2+1],prior=0;for(unsigned k=0;k<count;++k){auto ref=words[d.params[4]+start+k];assert(ref>=p->batches[n].first&&ref<p->batches[n].first+p->batches[n].count);if(k)assert(ref>prior);prior=ref;}}}
 auto invalid=*p;invalid.valid=false;assert(!gpueffects::pack(invalid,alphas,17,words,jobs));
 gpueffects::Builder off(v);off.line({-100,-100},{-20,-20},0xffffff);assert(off.finish()->commands.empty());
 gpueffects::Builder budget(v);for(size_t n=0;n<=gpueffects::MaximumCommands;++n)budget.pixel(0,0,1);assert(!budget.finish()->valid);
 auto resource=std::make_shared<gpupresent::Resource>();resource->width=resource->height=8;auto frame=std::make_shared<gpupresent::Frame>();frame->resource=resource;
 overlaycomposition::Context context{1,2,3,4,8,8,0,0};overlaycomposition::Surface s;s.id=1;s.context=context;s.rect={0,0,8,8};s.gpu=frame;assert(overlaycomposition::valid(s,context));std::vector<uint32_t>pixels;assert(!overlaycomposition::compose(context,{s},pixels));s.rect.w=9;assert(!overlaycomposition::valid(s,context));
 overlaycomposition::Surface edge;edge.id=2;edge.context=context;edge.rect={2,2,2,2};edge.clip={0,0,8,8};edge.extendEdges=true;edge.pixels=std::make_shared<const std::vector<uint32_t>>(std::initializer_list<uint32_t>{0xff112233,0xff445566,0xff778899,0xffaabbcc});assert(overlaycomposition::compose(context,{edge},pixels));assert(pixels.front()==0xff112233&&pixels.back()==0xffaabbcc&&pixels[2*8+3]==0xff445566);for(auto c:pixels)assert(c>>24==255);
 std::cout<<"PASS "<<cases<<" Bresenham/thickness coverage and blend multiplicity cases; effect ordering, wave snapshots, bounded CSR bins, black points, command overflow and immutable GPU surface contract; no Vulkan execution\n";
}
