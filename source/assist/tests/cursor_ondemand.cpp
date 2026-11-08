#include "../../cursor/source/ondemand.h"
#include <iostream>
#include <stdexcept>
static unsigned checks=0;
static void check(bool value,const char*reason){++checks;if(!value)throw std::runtime_error(reason);}
static unsigned pair(unsigned a,unsigned b){return a<<16|b;}
int main(){
 using namespace cursorondemand;
 Queue q;check(q.empty()&&!q.blocked(),"first run has no work; merely starting must not download");
 check(!q.request(pair(0,0),450),"static cursor has no interpolation edge");
 check(!q.request(pair(449,450),450),"source frame bound");
 check(!q.request(pair(65535,0),450),"untrusted high frame index");
 for(unsigned i=0;i<10;++i)check(q.request(pair(i,i+1),450),"observe distinct valid edge");
 check(!q.request(pair(3,4),450)&&q.size()==10,"repeated display of one edge queues only once");
 auto first=q.take();check(first.size()==4&&q.size()==6,"one automatic batch contains at most four edges");
 check(!q.request(first.front(),450),"submitted edge must not immediately retrigger");
 q.discardReady([](unsigned p){return (p>>16)%2==0;});
 check(q.size()==3,"disk-load completion removes requests already present in the resident bank");
 auto second=q.take();check(second.size()==3&&q.empty(),"only genuinely missing edges remain");
 check(q.request(pair(20,21),450),"new edge is admitted after successful earlier work");
 q.block();check(q.blocked()&&q.take().empty()&&!q.request(pair(21,22),450),"failure/cancel never retries itself");
 q.reset();check(!q.blocked()&&q.empty()&&q.request(first.front(),450),"explicit retry or changed source clears prior failure and attempts");
 check(q.take(0).empty()&&q.size()==1,"zero work budget does not consume a request");
 q.reset();for(unsigned i=0;i<MaxPairs;++i)check(q.request(pair(i,i+1),517),"517-frame source remains bounded");
 check(!q.request(pair(516,0),517)&&q.size()==MaxPairs,"pending and attempted work share the fixed cap");
 while(!q.empty()){auto batch=q.take();check(!batch.empty()&&batch.size()<=4,"bounded batches make progress");}
 check(!q.request(pair(516,0),517),"processed entries cannot cause unbounded request history");
 check(requestJson(Mode::All)=="{\"schema\":1,\"mode\":\"all\"}\n","all request has no pairs field");
 check(requestJson(Mode::Preview)=="{\"schema\":1,\"mode\":\"preview\"}\n","preview requires explicit mode");
 check(requestJson(Mode::Pairs,{pair(0,1),pair(516,0)})=="{\"schema\":1,\"mode\":\"pairs\",\"pairs\":[[0,1],[516,0]]}\n","literal numeric JSON preserves requested edges");
 using Bytes=std::vector<uint8_t>;
 Bytes art{1,2,3},colors{4,5},groups{6};const auto original=sourceKey(art,colors,groups);
 check(original.size()==32&&original.find_first_not_of(L"0123456789abcdef")==std::wstring::npos,"cache directory is a bounded, path-safe content key");
 check(sourceKey(art,colors,groups)==original,"identical content reuses its persistent directory");
 art[1]^=1;check(sourceKey(art,colors,groups)!=original,"changed game source has a distinct key");art[1]^=1;
 colors[0]^=1;check(sourceKey(art,colors,groups)!=original,"palette-only change invalidates the key");colors[0]^=1;
 groups.push_back(7);check(sourceKey(art,colors,groups)!=original,"group changes invalidate the key");groups.pop_back();
 check(sourceKey(Bytes{1},Bytes{2,3,4,5},groups)!=original,"length prefixes distinguish differently partitioned equal byte sequences");
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":0,\"scope\":\"actual bounded queue, request JSON and source identity; no Windows or game execution\"}\n";
}
