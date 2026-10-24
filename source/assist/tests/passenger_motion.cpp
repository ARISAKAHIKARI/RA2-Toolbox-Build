#include "../source/passenger_motion.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace assist;
int main(){
 static passengers::Motion cargo;
 static visual::Motion carrier,enemy;
 static Snapshot s;
 static visual::LabelView a[MaxLabels],b[MaxLabels];
 std::vector<external::passengers::Bubble> bubbles(1);
 bubbles[0].id=7;s.match=3;s.labels=1;
 unsigned fractions=0;
 for(unsigned n=0;n<1000;++n){
  s.frame=10+int(n/8);s.sampledTick=1000+2*n;
  s.cameraX=int(n/31);s.cameraY=int(n/41);
  int x=100+int(n/8)*3-s.cameraX,y=150+int(n/8)*2-s.cameraY;
  s.label[0]={7,x,y,1,4};
  bubbles[0].x=x;bubbles[0].y=y;
  auto* v=cargo.update(bubbles,s.match,s.frame,s.sampledTick,s.sampledTick,s.cameraX,s.cameraY);
  carrier.update(s);enemy.update(s);carrier.render(s.sampledTick,a);enemy.render(s.sampledTick,b);
  assert(v[0].x==a[0].x&&v[0].y==a[0].y&&v[0].x==b[0].x&&v[0].y==b[0].y);
  auto box=passengers::bounds(v[0].x,v[0].y,100,46,1200);
  assert(box.left==v[0].x-50&&box.top==v[0].y-72&&box.anchor==v[0].x);
  fractions+=box.left!=std::floor(box.left);
 }
 assert(fractions>200); // No integer layout step reintroduces a low-rate jump.
 // Camera jumps and teleports use the same immediate reset as other labels.
 bubbles[0].x=500;bubbles[0].y=400;
 auto*v=cargo.update(bubbles,9,1,5000,5000,0,0);
 assert(v[0].x==500&&v[0].y==400);
 bubbles.clear();cargo.update(bubbles,9,1,5002,5002,0,0);cargo.clear();
 bubbles.resize(1);bubbles[0].id=8;bubbles[0].x=30;bubbles[0].y=40;
 v=cargo.update(bubbles,9,1,5004,5004,0,0);
 assert(v[0].id==8&&v[0].x==30&&v[0].y==40);
 auto edge=passengers::bounds(2,20,100,46,600);assert(edge.left==0&&edge.top==0&&edge.anchor==8);
 std::cout<<"PASS 1000 shared 2ms samples: passenger bubble, carrier label and enemy anchor agree; subpixel layout, camera, epoch and disappearance\n";
}
