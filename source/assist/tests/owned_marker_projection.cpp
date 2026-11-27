#include "../../external_reader/source/owned_marker_projection.h"
#include "../source/enemy_clusters.h"
#include <cassert>
#include <cmath>
using namespace external;
struct View {uint32_t id=0;float x=0,y=0;};
static enemies::Frame f,out;
static View views[3],grouped[3];
int main(){
 Projection p{100,50,1024,768,.0866025403784};
 int ax=0,ay=0,x=0,y=0;
 assert(ownedmarkers::anchor(p,2048,1024,0,ax,ay));
 assert(project(p,2048,1024,0,x,y));
 assert(ownedmarkers::screen(ax,p.cameraX)==x&&ownedmarkers::screen(ay,p.cameraY)==y);
 p.cameraX+=90;p.cameraY-=20;
 assert(ownedmarkers::screen(ax,p.cameraX)==x-90&&ownedmarkers::screen(ay,p.cameraY)==y+20);
 int bx=0,by=0;
 assert(ownedmarkers::anchor(p,2048,1024,0,bx,by)&&ax==bx&&ay==by);
 assert(ownedmarkers::anchor(p,100000,100000,0,bx,by)); // off screen is not missing data
 assert(!ownedmarkers::anchor(p,INT32_MIN,0,0,bx,by));
 assert(ownedmarkers::local(7,7)&&!ownedmarkers::local(8,7)&&!ownedmarkers::local(0,0));
 f.house=7;f.match=1;f.count=3;
 for(int i=0;i<3;++i){auto&m=f.items[i];m.id=i+1;m.owner=7;m.type=i==2?2:1;m.kind=1;m.worldX=256*i;m.worldY=0;views[i]={m.id,float(i*60),float(i*30)};}
 assist::enemyclusters::GroupCache cache;
 assert(cache.update(f).size()==2);
 assert(assist::enemyclusters::render(f,views,3,cache.update(f),out,grouped)==2);
 assert(out.items[0].quantity==2&&grouped[0].x==30);
 for(auto&v:views){v.x-=90;v.y+=20;}
 assert(assist::enemyclusters::render(f,views,3,cache.update(f),out,grouped)==2);
 assert(grouped[0].x==-60&&grouped[0].y==35&&out.items[0].quantity==2);
}
