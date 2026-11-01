#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/enemy_clusters.h"
#include <cassert>
#include <cstdio>
#include <random>
using namespace assist::enemyclusters;
using Frame=external::enemies::Frame;
static Frame f;
static void add(int x,int y,int kind=1,unsigned owner=1,unsigned type=1){auto&m=f.items[f.count];m={};m.id=++f.count;m.object=0x10000+16*m.id;m.worldX=x;m.worldY=y;m.kind=kind;m.owner=owner;m.type=type;}
static std::vector<int> labels(const std::vector<Group>&groups,int count){std::vector<int> out(count,-1);for(size_t n=0;n<groups.size();++n)for(int i:groups[n].members){assert(i>=0&&i<count&&out[i]==-1);out[i]=int(n);}for(int n:out)assert(n>=0);return out;}
int main(){
 add(0,0);add(6*256,0);add(3*256,0);assert(group(f).size()==1); // late bridge, exact boundary
 f={};add(0,0);add(3*256+1,0);assert(group(f).size()==2);
 f={};add(0,0);add(3*256,1);assert(group(f).size()==2); // Euclidean, not square radius
 f={};add(0,0);add(0,0,1,2);add(0,0,1,1,2);add(0,0,3);add(0,0,3);add(-1,0);assert(group(f).size()==6);
 // Compare the spatial index against an independent all-pairs flood fill.
 std::mt19937 rng(711);unsigned comparisons=0;
 for(int trial=0;trial<120;++trial){f={};int n=32+int(rng()%96);for(int i=0;i<n;++i)add(int(rng()%8192),int(rng()%8192),int(rng()%4),1+rng()%2,1+rng()%3);
  auto actual=labels(group(f),n);std::vector<int> expected(n,-1),todo;
  for(int seed=0;seed<n;++seed)if(expected[seed]<0){expected[seed]=seed;todo={seed};for(size_t k=0;k<todo.size();++k){const auto&a=f.items[todo[k]];for(int j=0;j<n;++j){const auto&b=f.items[j];if(expected[j]>=0||a.kind>=3||a.kind!=b.kind||a.owner!=b.owner||a.type!=b.type)continue;int64_t dx=int64_t(a.worldX)-b.worldX,dy=int64_t(a.worldY)-b.worldY;if(dx*dx+dy*dy<=int64_t(768)*768){expected[j]=seed;todo.push_back(j);}}}}
  for(int a=0;a<n;++a)for(int b=0;b<n;++b){assert((actual[a]==actual[b])==(expected[a]==expected[b]));++comparisons;}
 }
 f={};for(int i=0;i<external::enemies::Maximum;++i)add(i%32,i/32);auto dense=group(f);assert(dense.size()==1&&dense[0].members.size()==external::enemies::Maximum);
 std::printf("PASS three-cell connected components: boundaries, late bridge, categories and %u flood-fill comparisons.\n",comparisons);
}
