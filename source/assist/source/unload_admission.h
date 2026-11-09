#pragma once
#include "object_index.h"
namespace assist { namespace unloadadmission {
constexpr int Capacity=8192;
struct Carrier {uint32_t pointer=0,id=0,token=0,attemptedAt=0;bool attempted=false;};
// Runtime-only storage, separate from both passenger planning batches and the
// game's event budget. Swap removal keeps cancellation and admission bounded.
struct Queue {
 Carrier carriers[Capacity]{};ObjectIndex index;
 int count=0,cursor=0,frame=-1;uint32_t epoch=0,house=0,lastProgress=0;
 void clear(){count=cursor=0;frame=-1;index.clear();}
 void begin(uint32_t scene,uint32_t owner,uint32_t now){if(epoch!=scene||house!=owner){clear();epoch=scene;house=owner;}lastProgress=now;}
 int find(uint32_t pointer,uint32_t id)const{uint32_t at=index.byId(id);return at&&at<=uint32_t(count)&&carriers[at-1].id==id&&carriers[at-1].pointer==pointer?int(at-1):-1;}
 bool add(Carrier value){int at=find(value.pointer,value.id);if(at<0){if(count==Capacity)return false;at=count++;}carriers[at]=value;index.add(value.pointer,value.id,uint32_t(at+1));return true;}
 void remove(int at){if(at<0||at>=count)return;carriers[at]=carriers[--count];if(at<count)index.add(carriers[at].pointer,carriers[at].id,uint32_t(at+1));if(at<cursor)cursor=at;if(cursor>=count)cursor=0;}
 void cancel(uint32_t pointer,uint32_t id){remove(find(pointer,id));}
 void reindex(){index.clear();for(int i=0;i<count;++i)index.add(carriers[i].pointer,carriers[i].id,uint32_t(i+1));}
};
} }
