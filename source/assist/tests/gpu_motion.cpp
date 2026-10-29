#include "../../external_reader/source/gpu_motion.h"
#include <cassert>
#include <cstdio>
using namespace external;
using namespace vulkancompute;
static FrameJob frame(){
 FrameJob f;f.width=120;f.height=80;f.motion.context=std::make_shared<const int>(1);f.motion.x=10;f.motion.y=20;f.motion.scaleX=f.motion.scaleY=2;
 f.motion.depthFar=-100;f.motion.depthNear=100;f.motion.tick=100;f.motion.frame=1;f.motion.terrainRevision=3;
 auto pixels=std::make_shared<const std::vector<uint32_t>>(4,0xff112233u);
 for(uint32_t n=0;n<2;++n){DrawJob d;d.source={pixels,{},{},2,2};d.left=20+20*float(n);d.top=10+10*float(n);d.motion={0x10000u+n*0x100,7+n,0x20000,2,{},true};f.draws.push_back(d);}return f;
}
static void reset(const FrameJob&a,const FrameJob&b){gpumotion::History h;gpumotion::Plan p;h.commit(a);assert(h.prepare(b,p)&&p.reset);for(auto mv:p.draws)assert(mv[0]==0&&mv[1]==0);}
int main(){
 auto a=frame();gpumotion::History h;gpumotion::Plan p;
 assert(h.prepare(a,p)&&p.reset&&p.eligible[0]&&p.eligible[1]);h.commit(a);
 auto b=a;b.motion.x=13;b.motion.y=22;b.motion.tick+=16;++b.motion.frame;
 b.draws[0].left=16;b.draws[0].top=6;b.draws[1].left=32;b.draws[1].top=18;
 assert(h.prepare(b,p)&&!p.reset&&p.base[0]==6&&p.base[1]==4);
 // Both instances share exactly the same immutable pose pixels. Their
 // distinct engine IDs still select different, exact rigid motion vectors.
 assert(p.draws[0][0]==4&&p.draws[0][1]==4&&p.draws[1][0]==8&&p.draws[1][1]==2);
 h.commit(b);auto order=b;std::reverse(order.draws.begin(),order.draws.end());order.motion.tick+=16;++order.motion.frame;
 order.draws[0].left-=3;order.draws[1].top+=2;assert(h.prepare(order,p)&&!p.reset);
 assert(p.draws[0][0]==3&&p.draws[0][1]==0&&p.draws[1][0]==0&&p.draws[1][1]==-2);
 auto change=b;change.draws[0].source.pixels=std::make_shared<const std::vector<uint32_t>>(4,0xff112233u);reset(a,change);
 change=b;change.draws[0].source.pixels={a.draws[0].source.pixels.get(),[](const std::vector<uint32_t>*){}};reset(a,change);
 change=b;change.draws[0].rasterScale=2;reset(a,change);
 change=b;change.draws[0].motion.type+=4;reset(a,change);
 change=b;change.draws.erase(change.draws.begin());reset(a,change);
 change=b;change.draws.push_back(change.draws.front());reset(a,change);
 h.clear();h.commit(a);assert(h.prepare(change,p)&&p.reset&&!p.eligible.front()&&!p.eligible.back());
 change=b;change.draws[0].motion.known=false;h.commit(a);assert(h.prepare(change,p)&&p.reset&&!p.eligible[0]);
 change=b;change.motion.context=std::make_shared<const int>(1);reset(a,change);
 change=b;change.motion.context={a.motion.context.get(),[](const void*){}};reset(a,change);
 change=b;++change.motion.terrainRevision;reset(a,change);
 change=b;change.motion.scaleX=1.5;reset(a,change);
 change=b;change.width=121;reset(a,change);
 change=b;change.motion.x=500;reset(a,change);
 change=b;change.motion.frame=0;reset(a,change);
 change=b;change.motion.tick=99;reset(a,change);
 change=b;change.motion.tick=351;reset(a,change);
 auto wrap=a;wrap.motion.tick=0xfffffff0u;h.commit(wrap);wrap.motion.tick=0x10;assert(h.prepare(wrap,p)&&!p.reset);
 auto parts=a;parts.draws[0].motion.part={0,1};parts.draws[1].motion=parts.draws[0].motion;parts.draws[1].motion.part={1,0};
 h.commit(parts);parts.motion.tick+=16;parts.draws[0].left-=1;parts.draws[1].left+=2;assert(h.prepare(parts,p)&&!p.reset&&p.draws[0][0]==1&&p.draws[1][0]==-2);
 auto invalid=a;invalid.motion.context.reset();assert(!h.prepare(invalid,p));h.commit(invalid);assert(h.prepare(a,p)&&p.reset);
 // The planner never owns the old source pixels merely to preserve history.
 std::weak_ptr<const std::vector<uint32_t>> weak;
 {auto temporary=frame();weak=temporary.draws[0].source.pixels;h.commit(temporary);}assert(weak.expired());
 std::puts("PASS: engine-ID rigid/camera motion, shared-pose instances, exact attachment paths, source/control-block reuse, context/pose/terrain/time resets and weak history ownership");
}
