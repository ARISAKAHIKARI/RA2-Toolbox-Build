#include "../source/label_motion.h"
#include "../source/clock_ms.h"
#include <cassert>
#include <cmath>
using namespace assist;
static visual::Motion motion;static Snapshot s;static visual::LabelView out[MaxLabels];
int main(){
 assert(visual::initialFPS(60,0)==500&&visual::initialFPS(240,0)==240&&visual::initialFPS(30,1)==30);
 assert(counterTick(1000,10000000,10000000,10020000,0)==1002); // real 2ms UI progression
 assert(counterTick(0xfffffff0u,100,1000,200,0)==84);assert(counterTick(10,100,0,200,42)==42);
 s.status=1;s.match=1;s.frame=1;s.sampledTick=100;s.labels=1;s.label[0]={7,100,200,1,5,150,0};
 motion.update(s);assert(motion.render(100,out)==1&&out[0].centis==1000);
 s.frame=2;s.sampledTick=150;s.label[0].x=108;s.label[0].ironFrames=149;s.label[0].used=2;motion.update(s);
 motion.render(175,out);assert(out[0].x==104&&out[0].used==2&&out[0].centis==997);
 motion.render(200,out);assert(out[0].x==108&&out[0].centis==993);
 int changes=0;float previous=-1;for(uint32_t now=150;now<=200;now+=2){motion.render(now,out);if(out[0].x!=previous)++changes;previous=out[0].x;}assert(changes>20); // 500Hz visual positions from two native snapshots
 motion.render(2000,out);assert(out[0].x==108&&out[0].centis==993); // paused simulation never runs past native remaining time
 s.sampledTick=160;s.cameraX=10;s.label[0].x=98;motion.update(s);
 motion.render(176,out);assert(out[0].x>=94&&out[0].x<=98); // camera correction is applied once, not to unit motion
 motion.render(250,out);assert(out[0].x==98&&out[0].centis==993);
 s.sampledTick=300;s.frame=3;s.label[0].ironFrames=200;motion.update(s);motion.render(300,out);assert(out[0].centis==1333); // renewed protection snaps
 s.frame=4;s.sampledTick=350;s.label[0].x=1000;motion.update(s);motion.render(350,out);assert(out[0].x==1000); // chrono jump snaps
 s.match=2;s.frame=1;s.sampledTick=400;s.labels=1;s.label[0]={9,50,60,0,0,15,1};motion.update(s);motion.render(400,out);assert(out[0].id==9&&out[0].centis==100&&out[0].shield==1);
 s.labels=0;s.sampledTick=450;motion.update(s);assert(motion.render(450,out)==0);
 s.match=3;s.frame=1;s.sampledTick=0xfffffff0u;s.labels=1;s.label[0]={5,0,0,0,0,15,0};motion.update(s);
 s.frame=2;s.sampledTick=20;s.label[0].x=8;s.label[0].ironFrames=14;motion.update(s);motion.render(38,out);assert(out[0].x==4);
 // Repeated samples/camera updates inside one simulation frame must not
 // shorten the NEXT frame's interpolation from 50 ms to an 8 ms burst.
 motion.clear();s={};s.match=9;s.frame=1;s.sampledTick=100;s.labels=1;s.label[0]={1,0,0};motion.update(s);
 s.sampledTick=116;motion.update(s);s.sampledTick=132;motion.update(s);s.sampledTick=148;motion.update(s);
 s.frame=2;s.sampledTick=150;s.label[0].x=8;motion.update(s);motion.render(175,out);assert(out[0].x==4);
 motion.render(149,out);assert(out[0].x==0); // no unsigned underflow on a future sample
 // Command bridge frames can be delayed: visual interpolation uses the
 // independently fresh reader frame, without rewriting command identity.
 motion.clear();s={};s.match=20;s.frame=100;s.sampledTick=100;s.labels=1;s.label[0]={1,0,0};motion.update(s,200);
 s.sampledTick=150;s.label[0].x=8;motion.update(s,201);motion.render(175,out);assert(out[0].x==4&&s.frame==100);
 // Fresh sub-frame positions should settle at the display sample interval,
 // not restart a whole simulation-frame delay on each sample.
 s.sampledTick=202;s.label[0].x=9;motion.update(s,201);s.sampledTick=204;s.label[0].x=10;motion.update(s,201);
 motion.render(206,out);assert(out[0].x==10);
 s.labels=MaxLabels;s.match=4;s.sampledTick=100;for(int i=0;i<MaxLabels;++i)s.label[i]={uint32_t(i+1),i,0,0,0,30,0};motion.update(s);
 s.frame=3;s.sampledTick=150;for(int i=0;i<MaxLabels;++i)s.label[i]={uint32_t(MaxLabels-i),MaxLabels-i+20,0,0,0,29,0};motion.update(s);assert(motion.render(200,out)==MaxLabels&&out[0].id==MaxLabels);
}
