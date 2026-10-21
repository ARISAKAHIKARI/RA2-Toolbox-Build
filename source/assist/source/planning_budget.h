#pragma once
#include <stdint.h>
namespace assist { namespace planningbudget {
// One frame allowance shared by BOTH external-planning mailboxes. Reservations
// protect repair/boarding/unload; each mailbox gets half while both are pending.
struct Budget {
 uint32_t match=0;int frame=-1,limit=0,used[3]{};
 void begin(uint32_t m,int f,int n){if(m!=match||f!=frame){match=m;frame=f;limit=n<0?0:n;used[0]=used[1]=used[2]=0;}}
 int room(int channel,bool otherPending)const{
  if(channel<0||channel>1)return 0;
  int left=limit-used[0]-used[1]-used[2],own=(otherPending?limit/2:limit)-used[channel];
  int n=left<own?left:own;return n>0?n:0;
 }
 int roomMask(int channel,uint32_t active)const{
  if(channel<0||channel>=3)return 0;active|=1u<<channel;
  int families=int(bool(active&1))+int(bool(active&2))+int(bool(active&4));
  int left=limit-used[0]-used[1]-used[2],own=limit/families-used[channel];
  int n=left<own?left:own;return n>0?n:0;
 }
 void consume(int channel){if(channel>=0&&channel<3&&used[0]+used[1]+used[2]<limit)++used[channel];}
};
}}
