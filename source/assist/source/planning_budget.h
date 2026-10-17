#pragma once
#include <stdint.h>
namespace assist { namespace planningbudget {
// One frame allowance shared by BOTH external-planning mailboxes. Reservations
// protect repair/boarding/unload; each mailbox gets half while both are pending.
struct Budget {
 uint32_t match=0;int frame=-1,limit=0,used[2]{};
 void begin(uint32_t m,int f,int n){if(m!=match||f!=frame){match=m;frame=f;limit=n<0?0:n;used[0]=used[1]=0;}}
 int room(int channel,bool otherPending)const{
  if(channel<0||channel>1)return 0;
  int left=limit-used[0]-used[1],own=(otherPending?limit/2:limit)-used[channel];
  int n=left<own?left:own;return n>0?n:0;
 }
 void consume(int channel){if(channel>=0&&channel<2&&used[0]+used[1]<limit)++used[channel];}
};
}}
