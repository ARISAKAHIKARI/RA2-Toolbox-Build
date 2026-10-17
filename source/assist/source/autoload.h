#pragma once
#include <stdint.h>
#include <algorithm>

namespace assist { namespace loading {
constexpr int MaxInfantry=512, MaxVehicles=128, GraceFrames=90;
struct Infantry {uint32_t object=0,id=0;int x=0,y=0,slots=0;uint32_t type=0;int kind=1;uint32_t ticket=0,queuedAt=0;};
struct Vehicle {uint32_t object=0,id=0;int x=0,y=0,capacity=0,used=0,reserved=0;double sizeLimit=128;int passengers=0;};
struct Observation {int mission=0,queued=-1;uint32_t target=0,destination=0;};
struct Reservation {uint32_t object,id,vehicle,vehicleID;int slots,frame;int lastSent=-1,x=0,y=0,progress=0,lastAttempt=-1,retryAfter=0,attempts=0;Observation initial{};bool enterObserved=false;int kind=1;uint32_t ticket=0,queuedAt=0;};
inline bool takenOver(Reservation&r,const Observation&now,uint32_t incoming){
 if(incoming&&incoming!=r.vehicle)return true;
 if(now.queued>=0&&now.queued!=7&&now.queued!=r.initial.queued)return true;
 if(incoming==r.vehicle&&(now.mission==7||now.queued==7)){r.enterObserved=true;return false;}
 if(now.mission==0||now.mission==5)return r.enterObserved;
 if(now.mission==7&&!incoming)return false;
 return r.enterObserved||now.mission!=r.initial.mission||now.target!=r.initial.target||now.destination!=r.initial.destination;
}
inline int slots(double size){
 // Vanilla sizes are integral. Round a mod's positive fraction upward so
 // an assignment cannot consume more space than we reserved.
 if(!(size>0&&size<=128))return 0;
 int n=int(size);return n+(size>n);
}
inline bool coordinate(int n){return n>=-1048576&&n<=1048576;}
inline int freeSlots(const Vehicle&v){
 if(v.capacity<1||v.capacity>1024||v.used<0||v.reserved<0)return 0;
 int64_t n=int64_t(v.capacity)-v.used-v.reserved;return n>0?int(n):0;
}
// Pure assignment policy shared by both modes. Geometry/size checks precede
// the original UI legality predicate. No mission, pathfinding or RNG callback.
struct Composition {
 uint32_t types[MaxInfantry]{};int count=0;int16_t numbers[MaxVehicles][MaxInfantry]{};
 void clear(){count=0;for(auto&v:numbers)for(auto&n:v)n=0;}
 int index(uint32_t type){for(int i=0;i<count;++i)if(types[i]==type)return i;if(count>=MaxInfantry)return -1;types[count]=type;return count++;}
 void add(int vehicle,uint32_t type){int i=index(type);if(vehicle>=0&&vehicle<MaxVehicles&&i>=0&&numbers[vehicle][i]<32767)++numbers[vehicle][i];}
};
struct Workspace {
 uint8_t nearRank[MaxInfantry][MaxVehicles];int nearAt[MaxInfantry];bool known[MaxInfantry];
 int sourceType[MaxInfantry],begin[MaxInfantry+1];
 uint16_t fairRank[MaxVehicles][MaxInfantry],fairAt[MaxInfantry][MaxVehicles];
 int8_t legality[MaxInfantry][MaxVehicles];
};
template<class Compatible> int assign(const Infantry*p,int ns,Vehicle*v,int nv,int mode,int radius,Composition&composition,Workspace&work,int*out,Compatible compatible){
 if(ns<0||ns>MaxInfantry||nv<0||nv>MaxVehicles)return 0;
 for(int i=0;i<ns;++i)out[i]=-1;
 int64_t limit=radius>0&&radius<=512?int64_t(radius)*256:131072;limit*=limit;
 auto distance=[&](int i,int j){if(!coordinate(p[i].x)||!coordinate(p[i].y)||!coordinate(v[j].x)||!coordinate(v[j].y))return INT64_MAX;int64_t dx=int64_t(p[i].x)-v[j].x,dy=int64_t(p[i].y)-v[j].y;return dx*dx+dy*dy;};
 if(mode!=1){
  // Capacity only decreases: each source's ranked carrier cursor advances.
  auto&rank=work.nearRank;auto&at=work.nearAt;auto&known=work.known;
  for(int i=0;i<ns;++i){at[i]=0;known[i]=false;}
  for(int i=0;i<ns;++i){for(int j=0;j<nv;++j)rank[i][j]=uint8_t(j);std::sort(rank[i],rank[i]+nv,[&](uint8_t a,uint8_t b){int64_t da=distance(i,a),db=distance(i,b);return da!=db?da<db:v[a].id<v[b].id;});}
  int assigned=0;
  for(;;){int bi=-1,bj=-1;int64_t best=INT64_MAX;
   for(int i=0;i<ns;++i){if(out[i]>=0||p[i].slots<=0)continue;
    while(at[i]<nv){int j=rank[i][at[i]];int64_t d=distance(i,j);bool valid=d<=limit&&freeSlots(v[j])>=p[i].slots&&v[j].sizeLimit>=p[i].slots;
     if(valid&&!known[i]){valid=compatible(p[i],v[j]);known[i]=valid;}
     if(!valid){++at[i];known[i]=false;continue;}
     if(bi<0||d<best||(d==best&&(p[i].id<p[bi].id||(p[i].id==p[bi].id&&v[j].id<v[bj].id)))){bi=i;bj=j;best=d;}break;
    }
   }
   if(bi<0)break;out[bi]=bj;v[bj].reserved+=p[bi].slots;++v[bj].passengers;composition.add(bj,p[bi].type);++assigned;
  }return assigned;
 }
 // Fair type turns compare one nearest eligible source per carrier.
 auto&sourceType=work.sourceType;auto&begin=work.begin;for(auto&n:begin)n=0;
 for(int i=0;i<ns;++i){sourceType[i]=composition.index(p[i].type);if(sourceType[i]>=0)++begin[sourceType[i]+1];}
 for(int t=0;t<composition.count;++t)begin[t+1]+=begin[t];
 auto&rank=work.fairRank;auto&at=work.fairAt;auto&legality=work.legality;
 for(int i=0;i<ns;++i)for(int j=0;j<nv;++j)legality[i][j]=0;
 for(int j=0;j<nv;++j){for(int i=0;i<ns;++i)rank[j][i]=uint16_t(i);
  std::sort(rank[j],rank[j]+ns,[&](uint16_t a,uint16_t b){int ta=sourceType[a]<0?MaxInfantry:sourceType[a],tb=sourceType[b]<0?MaxInfantry:sourceType[b];if(ta!=tb)return ta<tb;int64_t da=distance(a,j),db=distance(b,j);return da!=db?da<db:p[a].id<p[b].id;});
  for(int t=0;t<composition.count;++t)at[t][j]=uint16_t(begin[t]);
 }
 int assigned=0;bool progress=true;
 while(progress){progress=false;
  for(int type=0;type<composition.count;++type){int bi=-1,bj=-1,same=0,total=0;int64_t best=INT64_MAX;
   for(int j=0;j<nv;++j){auto&position=at[type][j];
    while(position<begin[type+1]){int i=rank[j][position];int64_t d=distance(i,j);
     bool valid=out[i]<0&&p[i].slots>0&&freeSlots(v[j])>=p[i].slots&&v[j].sizeLimit>=p[i].slots&&d<=limit;
     if(valid){if(!legality[i][j])legality[i][j]=compatible(p[i],v[j])?1:-1;valid=legality[i][j]>0;}
     if(!valid){++position;continue;}
     int n=composition.numbers[j][type],t=v[j].passengers;
     if(bi<0||n<same||(n==same&&(t<total||(t==total&&(d<best||(d==best&&(p[i].id<p[bi].id||(p[i].id==p[bi].id&&v[j].id<v[bj].id)))))))){bi=i;bj=j;same=n;total=t;best=d;}break;
    }
   }
   if(bi<0)continue;out[bi]=bj;v[bj].reserved+=p[bi].slots;++v[bj].passengers;composition.add(bj,p[bi].type);++assigned;progress=true;
  }
 }return assigned;
}

template<class Compatible> int nearest(const Infantry&p,const Vehicle*v,int n,Compatible compatible){
 if(p.slots<=0||!coordinate(p.x)||!coordinate(p.y))return -1;
 int best=-1;uint64_t distance=~uint64_t(0);
 for(int i=0;i<n;++i){
  if(!coordinate(v[i].x)||!coordinate(v[i].y)||freeSlots(v[i])<p.slots)continue;
  int64_t dx=int64_t(p.x)-v[i].x,dy=int64_t(p.y)-v[i].y;
  uint64_t d=uint64_t(dx*dx+dy*dy);
  if((d<distance||(d==distance&&(best<0||v[i].id<v[best].id)))&&compatible(p,v[i])){best=i;distance=d;}
 }return best;
}
template<int N=MaxInfantry> struct Ledger {
 Reservation records[N]{};int count=0;
 void clear(){count=0;}
 bool contains(uint32_t id)const{for(int i=0;i<count;++i)if(records[i].id==id)return true;return false;}
 bool add(const Reservation&r){if(count>=N||contains(r.id))return false;records[count++]=r;return true;}
 void erase(int i){if(i>=0&&i<count)records[i]=records[--count];}
};
} }
