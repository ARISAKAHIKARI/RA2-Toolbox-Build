#include "../../external_reader/source/material_motion.h"
#include "../../external_reader/source/material_blend.h"
#include <cassert>
#include <cstdio>
using namespace external;
static radar::Frame bank(int frame,int x){
 radar::Frame f;auto t=std::make_shared<radar::Terrain>();t->geo={8,8,6,7,8,8,1,0x10000,0x20000,512*512,{8,8,0,0,8,8},8,8};f.terrain=t;f.frame=frame;f.markerScanValid=true;
 radar::Marker m;m.object=0x30000;m.id=1;m.type=0x40000;m.owner=0x20000;m.kind=2;m.x=x;m.y=1024;m.pose.frame=10;m.pose.rawFacing=true;m.pose.rawBody=65024;m.pose.rawTurret=65024;m.pose.body=0;m.pose.turret=0;m.pose.voxel=true;f.markers={m};return f;
}
int main(){
 materialmotion::Timeline timeline;auto a=bank(100,1024),b=bank(101,1088);b.posesPrepared=true;
 b.markers[0].pose.frame=11;b.markers[0].pose.rawBody=512;b.markers[0].pose.rawTurret=1024;
 assert(timeline.observe(a,1000)&&timeline.observe(b,1020));
 auto start=timeline.sample(1020),middle=timeline.sample(1030),end=timeline.sample(1040);
 assert(start.markers[0].x==1024&&middle.markers[0].x==1056&&end.markers[0].x==1088);
 assert(middle.markers[0].pose.bodyFine==0&&middle.markers[0].pose.turretFine==1); // shortest wrapped turn
 assert(start.markers[0].pose.frameFrom==10&&start.markers[0].pose.frameWeight==0);
 assert(middle.markers[0].pose.frameWeight==128&&end.markers[0].pose.frameFrom==-1);
 assert(timeline.sample(1100).markers[0].x==1088); // paused engine: no extrapolation
 assert(!timeline.observe(b,1100)&&timeline.interval()==20); // same tick cannot reset interpolation
 assert(timeline.sample(1030,false).markers[0].x==1088);
 assert(timeline.sample(1030,true,false).markers[0].pose.bodyFine==-1);
 // Live presenters have no observation-buffer delay, including a fresh
 // memory update whose native frame number has not advanced yet.
 auto current=materialmotion::latest(b);assert(current.markers[0].x==1088&&current.markers[0].pose.frameFrom==-1);
 b.markers[0].x=1099;b.markers[0].pose.rawBody=1024;
 for(int n=0;n<5;++n){current=materialmotion::latest(b);assert(current.frame==101&&current.markers[0].x==1099&&current.markers[0].pose.bodyFine==4);}
 b.markers[0].x=1088;
 assert(materialmotion::presentationKey(start)!=materialmotion::presentationKey(middle));
 assert(materialmotion::presentationKey(end)==materialmotion::presentationKey(timeline.sample(1100)));
 auto gone=bank(102,1100);gone.markers.clear();assert(timeline.observe(gone,1040));assert(timeline.sample(1041).markers.empty());
 auto recycled=bank(103,3000);recycled.markers[0].id=2;timeline.observe(recycled,1060);assert(timeline.sample(1061).markers[0].x==3000);
 auto teleport=recycled;teleport.frame=104;teleport.markers[0].x=6000;timeline.observe(teleport,1080);assert(timeline.sample(1081).markers[0].x==6000);
 auto owner=teleport;owner.frame=105;owner.markers[0].x=6050;owner.markers[0].owner=0x90000;timeline.observe(owner,1100);assert(timeline.sample(1101).markers[0].x==6050);
 auto bridge=owner;bridge.frame=106;bridge.markers[0].onBridge=true;bridge.markers[0].x=6100;timeline.observe(bridge,1120);assert(timeline.sample(1121).markers[0].x==6100);
 auto stale=bridge;stale.frame=107;stale.markers[0].x=6150;timeline.observe(stale,1500);assert(timeline.sample(1501).markers[0].x==6150);
 auto reset=bank(1,1000);timeline.observe(reset,1520);assert(timeline.sample(1521).markers[0].x==1000);
 auto invalid=bank(2,1050);invalid.markerScanValid=false;assert(!timeline.observe(invalid,1540));
 auto sequence=reset;sequence.frame=2;sequence.markers[0].pose.frame=500;timeline.observe(sequence,1540);assert(timeline.sample(1541).markers[0].pose.frameFrom==-1);
 // Adjacent numeric SHP indices can belong to unrelated actions or facing
 // bins. Movement remains smooth, while those discrete pictures must snap.
 auto transition=[&](int kind,int beforeSequence,int afterSequence,int beforeBody,int afterBody,int beforeTurret,int afterTurret){
  materialmotion::Timeline discrete;auto old=bank(200,1024),next=bank(201,1088);
  old.markers[0].kind=next.markers[0].kind=kind;old.markers[0].pose.voxel=next.markers[0].pose.voxel=false;
  auto&ap=old.markers[0].pose;auto&bp=next.markers[0].pose;ap.frame=10;bp.frame=11;
  ap.sequence=beforeSequence;bp.sequence=afterSequence;ap.body=uint8_t(beforeBody);bp.body=uint8_t(afterBody);ap.turretFrame=beforeTurret;bp.turretFrame=afterTurret;
  assert(discrete.observe(old,2000)&&discrete.observe(next,2020));return discrete.sample(2030).markers[0];
 };
 auto changedAction=transition(1,1,5,0,0,-1,-1);assert(changedAction.pose.frameFrom==-1&&changedAction.x==1056);
 assert(transition(2,3,1,0,0,-1,-1).pose.frameFrom==-1); // walk to fire
 assert(transition(projectiles::Kind,0,0,10,9,-1,-1).pose.frameFrom==-1); // rotating SHP shot
 assert(transition(2,2,2,0,0,10,11).pose.frameFrom==-1); // independent SHP turret
 assert(transition(1,5,5,0,0,-1,-1).pose.frameFrom==10); // true same-action animation
 auto loop=reset;loop.frame=3;loop.markers[0].pose.frame=0;timeline.observe(loop,1560);assert(timeline.sample(1570).markers[0].pose.frameFrom==-1);
 auto nan=timeline.sample(std::numeric_limits<double>::quiet_NaN());assert(!nan.terrain);
 assert(materialmotion::refreshPeriod(500,true)==2&&materialmotion::refreshPeriod(500,false)==16&&materialmotion::refreshPeriod(-1,true)==200);
 voxel::Image first,second,blend;first.width=second.width=2;first.height=second.height=1;
 first.pixels={0xffff0000,0};second.pixels={0,0xff0000ff};first.x=-1;second.x=0;
 assert(materialblend::image(first,second,128,1,blend));assert(blend.x==-1&&blend.width==3);
 assert(blend.pixels[0]>>24==127&&blend.pixels[2]>>24==128);
 for(auto p:blend.pixels)for(int shift:{0,8,16})assert(((p>>shift)&255)<=(p>>24));
 voxel::Image exact;assert(materialblend::image(first,second,0,1,exact)&&exact.pixels==first.pixels);
 assert(materialblend::image(first,second,255,1,exact)&&exact.pixels==second.pixels);
 second.depthBias=20;assert(!materialblend::image(first,second,128,1,blend));
 std::puts("PASS: observed-only motion/turn/SHP interpolation; pause, disappearance, identity, bridge, teleport, lag and clock reset guards; aligned premultiplied frame blend");
}
