#include "../source/enemy_clusters.h"
#include "../source/label_motion.h"
#include <cassert>
#include <iostream>
using namespace assist;using namespace external::enemies;
static Frame raw,out;static visual::LabelView positions[Maximum],result[Maximum];
static void add(int x,int y,uint32_t id,uint32_t owner=1,uint32_t type=1,int kind=2){
 auto&m=raw.items[raw.count++];m={};m.id=id;m.object=id+0x10000;m.owner=owner;m.type=type;m.kind=kind;m.worldX=x;m.worldY=y;m.x=x/4;m.y=y/4;
}
static auto groups(){return enemyclusters::group(raw);}
int main(){
 raw.width=1920;raw.height=1080;add(1000,1000,3);add(1512,1000,1);assert(groups().size()==1);
 raw.items[1].worldX=1513;assert(groups().size()==2);raw.items[1].worldX=1512;
 add(2024,1000,2);auto g=groups();assert(g.size()==2); // no transitive chain beyond two cells
 for(auto&a:g)for(int i:a.members)for(int j:a.members)assert(enemyclusters::square(int64_t(raw.items[i].worldX)-raw.items[j].worldX)+enemyclusters::square(int64_t(raw.items[i].worldY)-raw.items[j].worldY)<=512*512);
 raw.count=0;add(1000,1000,1);add(1000,1000,2,2);add(1000,1000,3,1,2);add(1000,1000,4,1,1,3);add(1000,1000,5,1,1,3);assert(groups().size()==5);
 raw.count=0;add(1000,1000,30);add(1362,1362,10);assert(groups().size()==1);raw.items[1].worldX=1363;assert(groups().size()==2);
 raw.count=0;add(1000,1000,1);add(1100,1000,2);add(5000,5000,3);g=groups();
 for(int i=0;i<3;++i){positions[i]={};positions[i].id=raw.items[i].id;positions[i].x=100.25f+i*20;positions[i].y=50.5f+i*10;}
 assert(enemyclusters::render(raw,positions,3,g,out,result)==2&&out.items[0].quantity==2&&out.items[1].quantity==1);
 assert(result[0].x==110.25f&&result[0].y==55.5f);
 // Unordered Motion output is matched by identity, with no camera-scale distance.
 std::swap(positions[0],positions[2]);enemyclusters::render(raw,positions,3,g,out,result);assert(result[0].x==110.25f);
 for(int step=0;step<1000;++step){for(int i=0;i<3;++i){positions[i].x+=.125f;positions[i].y-=.25f;}enemyclusters::render(raw,positions,3,g,out,result);assert(result[0].x==110.25f+(step+1)*.125f);}
 raw.items[1].worldX=1513;g=groups();assert(g.size()==3);enemyclusters::render(raw,positions,3,g,out,result);assert(result[0].x==positions[2].x&&out.items[0].quantity==1);
 raw.items[1].worldX=1100;std::swap(raw.items[0],raw.items[2]);g=groups();enemyclusters::render(raw,positions,3,g,out,result);assert(out.items[0].id==1&&out.items[0].quantity==2);
 // The maximum crowded army is one compact group; bad coordinates never merge.
 raw.count=0;for(int i=0;i<Maximum;++i)add(1000+i%100,1000+i%73,i+1);assert(groups().size()==1);
 raw.items[0].worldX=INT32_MIN;assert(groups().size()==2);
 raw.count=0;assert(groups().empty());
 std::cout<<"PASS exact 2-cell distance, owner/type separation, no chain merge, deterministic ordering, 2048 units, per-unit smooth centroid and immediate split\n";
}
