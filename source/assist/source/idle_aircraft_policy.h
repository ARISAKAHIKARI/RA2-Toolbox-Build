#pragma once
namespace assist { namespace idleaircraft {
struct View {int ammo=0,maximum=0,mission=-1,queued=-1;bool valid=false,airborne=false,target=false,docked=false,returning=false;};
inline bool idle(const View&v){
 if(!v.valid||v.maximum<=0||v.ammo<v.maximum)return false;
 if(v.queued>=0&&v.queued!=v.mission&&v.queued!=5&&v.queued!=11)return false;
 if(v.returning&&v.mission==7)return true; // native Enter to a compatible airport
 return !v.target&&!v.airborne&&v.docked&&(v.mission==0||v.mission==5||v.mission==11||v.mission==13);
}
}}
