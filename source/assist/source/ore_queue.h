#pragma once
#include <stdint.h>
namespace assist { namespace ore {
constexpr int MaxJobs=256;
inline double storageAmount(const float(&values)[4]){double total=0;for(float value:values){if(!(value>=0&&value<1000000))return -1;total+=value;}return total;}
constexpr int DockStallFrames=180,MaxDockAttempts=3;
constexpr int NearDockCells=2,FarDockCells=4,YieldRetryFrames=30,YieldTimeoutFrames=180,TurnTimeoutFrames=360;
// Shared geometry only: the external planner decides, the runtime rechecks.
// Use the same native dock origin as the read-only waiting-cell sampler.
inline int64_t dockDistance(int x,int y,int refineryX,int refineryY,bool styleB){
 int64_t dx=int64_t(x)/256-(int64_t(refineryX)/256+(styleB?2:1));
 int64_t dy=int64_t(y)/256-(int64_t(refineryY)/256+2);
 if(dx<0)dx=-dx;if(dy<0)dy=-dy;return dx>dy?dx:dy;
}
enum Stage { Waiting, Approaching, Docking, Yielding };
struct Job {
 uint32_t unit=0,id=0,refinery=0,refineryID=0,waitCell=0,ticket=0;
 int stage=Waiting,created=0,sentFrame=-1,observed=0;
};
struct Queue {
 Job jobs[MaxJobs]{};int count=0;uint32_t nextTicket=0;
 void clear(){count=0;nextTicket=0;}
 int find(uint32_t id)const{for(int i=0;i<count;++i)if(jobs[i].id==id)return i;return -1;}
 bool add(Job job){if(find(job.id)>=0)return true;if(count>=MaxJobs)return false;job.ticket=++nextTicket;jobs[count++]=job;return true;}
 void remove(int index){for(int i=index+1;i<count;++i)jobs[i-1]=jobs[i];--count;}
 bool first(int index)const{for(int i=0;i<index;++i)if(jobs[i].refineryID==jobs[index].refineryID)return false;return true;}
 int position(int index)const{int n=0;for(int i=0;i<index;++i)if(jobs[i].refineryID==jobs[index].refineryID)++n;return n;}
 bool cellReserved(uint32_t cell,int ignore)const{for(int i=0;i<count;++i)if(i!=ignore&&jobs[i].waitCell==cell&&jobs[i].stage!=Docking)return true;return false;}
};
inline bool full(double amount){return amount>=1.0;}
inline bool automaticMission(int mission){return mission==5||mission==10||mission==12||mission==16;}
}}
