#pragma once
#include <cstddef>
namespace assist { namespace destructownerhost {
constexpr int Hotkey=1735;
// This destructive action always owns a separate physical chord, regardless
// of the ordinary "all matching actions" preference. Never place it in the
// generic command backlog or resolve an old key edge against a new selection.
template<class Group,class Immediate>bool immediate(Group&group,Immediate action){
 bool found=false;for(int id:group.actions)found|=id==Hotkey;
 if(!found)return false;
 const bool exclusive=group.actions.size()==1;
 group.actions.clear();group.bindings.clear();
 if(exclusive)action();return true;
}
template<class Registry>bool exclusive(const Registry&registry,unsigned chord){
 const unsigned physical=chord&0x7ff;
 for(const auto&group:registry.groups)if(group.key==physical)return false;
 return (physical&255)!=0;
}
}}
