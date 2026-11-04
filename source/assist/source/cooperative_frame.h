#pragma once
#include "cooperative_plan.h"
#include <cstring>
namespace assist { namespace cooperative {
// Fixed value-only frame capture. No game pointer is dereferenced by unpack.
struct FrameInput {
 Request request{};uint32_t match=0,tick=0;int frame=-1,mode=Enter,unitCount=0,targetCount=0,centerX=0,centerY=0,damageCount=0;
 Unit units[MaxSources];Target targets[MaxTargets];
 uint8_t compatible[MaxSources*MaxTargets];double expectedDamage[MaxSources*MaxTargets];
};
inline bool validFrame(const FrameInput&i){return valid(i.request)&&i.match==i.request.match&&i.frame>=0&&i.unitCount>=0&&i.unitCount<=MaxSources&&i.targetCount>=0&&i.targetCount<=MaxTargets&&(i.damageCount==0||i.damageCount==i.unitCount*i.targetCount);}
inline bool pack(FrameInput&out,const Input&i,const Request&r,uint32_t tick,int frame){
 if(i.units.size()>MaxSources||i.targets.size()>MaxTargets||i.compatible.size()!=i.units.size()*i.targets.size()||(!i.expectedDamage.empty()&&i.expectedDamage.size()!=i.compatible.size())){out.frame=-1;return false;}
 out.request=r;out.match=r.match;out.tick=tick;out.frame=frame;out.mode=i.mode;out.unitCount=int(i.units.size());out.targetCount=int(i.targets.size());out.centerX=i.centerX;out.centerY=i.centerY;out.damageCount=int(i.expectedDamage.size());
 if(out.unitCount)memcpy(out.units,i.units.data(),i.units.size()*sizeof(Unit));if(out.targetCount)memcpy(out.targets,i.targets.data(),i.targets.size()*sizeof(Target));
 if(!i.compatible.empty())memcpy(out.compatible,i.compatible.data(),i.compatible.size());if(out.damageCount)memcpy(out.expectedDamage,i.expectedDamage.data(),i.expectedDamage.size()*sizeof(double));return true;
}
inline Input unpack(const FrameInput&i){Input out;if(!validFrame(i))return out;out.mode=i.mode;out.clicked=i.request.target;out.centerX=i.centerX;out.centerY=i.centerY;out.units.assign(i.units,i.units+i.unitCount);out.targets.assign(i.targets,i.targets+i.targetCount);out.compatible.assign(i.compatible,i.compatible+size_t(i.unitCount)*i.targetCount);if(i.damageCount)out.expectedDamage.assign(i.expectedDamage,i.expectedDamage+i.damageCount);return out;}
inline bool copyFrame(FrameInput&out,const FrameInput&i){
 out.request=i.request;out.match=i.match;out.tick=i.tick;out.frame=i.frame;out.mode=i.mode;out.unitCount=i.unitCount;out.targetCount=i.targetCount;out.centerX=i.centerX;out.centerY=i.centerY;out.damageCount=i.damageCount;
 if(!validFrame(out))return false;const size_t cells=size_t(out.unitCount)*out.targetCount;
 if(out.unitCount)memcpy(out.units,i.units,size_t(out.unitCount)*sizeof(Unit));if(out.targetCount)memcpy(out.targets,i.targets,size_t(out.targetCount)*sizeof(Target));if(cells)memcpy(out.compatible,i.compatible,cells);if(out.damageCount)memcpy(out.expectedDamage,i.expectedDamage,size_t(out.damageCount)*sizeof(double));return true;
}
inline void restrictSources(Input&input,const Request&r){
 Input out;out.mode=input.mode;out.clicked=input.clicked;out.centerX=input.centerX;out.centerY=input.centerY;out.targets=std::move(input.targets);const size_t columns=out.targets.size();
 for(int n=0;n<r.count;++n)for(size_t i=0;i<input.units.size();++i)if(input.units[i].source.id==r.sources[n].id&&input.units[i].source.token==r.sources[n].token){out.units.push_back(input.units[i]);out.compatible.insert(out.compatible.end(),input.compatible.begin()+i*columns,input.compatible.begin()+(i+1)*columns);if(!input.expectedDamage.empty())out.expectedDamage.insert(out.expectedDamage.end(),input.expectedDamage.begin()+i*columns,input.expectedDamage.begin()+(i+1)*columns);break;}input=std::move(out);
}
}
}
