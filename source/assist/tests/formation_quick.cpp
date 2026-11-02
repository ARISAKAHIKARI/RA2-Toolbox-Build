#include "../source/formation_quick.h"
#include <cassert>
#include <map>
#include <tuple>
using namespace assist::formation;
int main(){
 Unit units[Maximum]{};Slot slots[Maximum]{};bool air[Maximum]{};
 for(int n=0;n<Maximum;++n){units[n].kind=n%3==0?1:2;units[n].x=(n%11)*256;units[n].y=(n/11)*256;air[n]=n%3==2;}
 auto valid=[](int x,int y,bool flying){return x>0&&y>0&&x<80&&y<80&&(flying||x!=20||y!=20);};
 quick::Path point;point.push(20,20);
 quick::layout(units,Maximum,air,point,20,20,slots,valid);
 std::map<std::tuple<int,int,int>,int> counts;
 for(int n=0;n<Maximum;++n){assert(slots[n].valid);int x=slots[n].x/256,y=slots[n].y/256;assert(valid(x,y,air[n]));int c=quick::category(units[n],air[n]);assert((++counts[{x,y,c}]<=quick::quota(c)));}
 quick::Path curve;curve.push(10,10);curve.push(20,10);curve.push(20,40);
 auto half=curve.at(.5);assert(std::abs(half.x-20)<.001&&std::abs(half.y-20)<.001);
 Unit group[4]{};bool groups[4]={false,false,true,true};Slot line[4]{};
 for(int n=0;n<4;++n){group[n].kind=2;group[n].x=5120;group[n].y=n%2?8960:3840;}
 quick::layout(group,4,groups,curve,0,0,line,[](int x,int y,bool){return x>0&&y>0;});
 // Both air and ground groups independently cover the entire curve.
 assert(line[0].x/256==20&&line[0].y/256==10);
 assert(line[1].x/256==20&&line[1].y/256==30);
 assert(line[2].x==line[0].x&&line[2].y==line[0].y);
 assert(line[3].x==line[1].x&&line[3].y==line[1].y);
 quick::layout(group,4,groups,curve,0,0,line,[](int,int,bool){return false;});
 for(auto&s:line)assert(!s.valid);
 quick::Path longPath;for(int n=0;n<200;++n)longPath.push(n*2,5);assert(longPath.count==quick::NodeLimit);assert(longPath.nodes[quick::NodeLimit-1].x==398);
 longPath.push(399,5,true);assert(longPath.nodes[quick::NodeLimit-1].x==399);
 assert(Shapes==6); // Configured key count and shared policy size stay stable.
}
