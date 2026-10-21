#pragma once
#include <stdint.h>
namespace assist { namespace planningbudget {
// The common frame allowance covers tactical, cooperative, missile, burst and
// dodge producers. Recurring repair/load/unload reservations remain upstream.
struct Budget {
 static constexpr int Channels=5;
 uint32_t match=0;int frame=-1,limit=0,used[Channels]{};
 int total()const{int n=0;for(int v:used)n+=v;return n;}
 void begin(uint32_t m,int f,int n){if(m!=match||f!=frame){match=m;frame=f;limit=n<0?0:n;for(int&v:used)v=0;}}
 int room(int channel,bool otherPending)const{
  if(channel<0||channel>1)return 0;int left=limit-total(),own=(otherPending?limit/2:limit)-used[channel];int n=left<own?left:own;return n>0?n:0;
 }
 int roomMask(int channel,uint32_t active)const{
  if(channel<0||channel>=Channels)return 0;active|=1u<<channel;int families=0;
  for(int k=0;k<Channels;++k)families+=bool(active&(1u<<k));
  int left=limit-total(),own=limit/families-used[channel];int n=left<own?left:own;return n>0?n:0;
 }
 void consume(int channel){if(channel>=0&&channel<Channels&&total()<limit)++used[channel];}
};
}}
