#pragma once
#include "native_placement_policy.h"
#include <array>
#include <cstddef>
namespace assist { namespace placementeventhost {
constexpr int Hotkey=1733;
// This is a product selection, never a pending placement. Every place action
// needs a new input edge and a fresh offer for the exact same native product.
struct Selection {
 std::array<nativeplacement::Offer,nativeplacement::MaxOffers> offers{};
 nativeplacement::Offer selected{};uint32_t pid=0,match=0;int count=0;bool chosen=false;
 void clear(){*this={};}
 void update(uint32_t process,uint32_t epoch,const nativeplacement::Offer*items,int size,uint32_t now,int frame,uint64_t hash,bool available){
  if(!available||!process||!epoch||size<0||size>nativeplacement::MaxOffers){clear();return;}
  if(pid!=process||match!=epoch)clear();pid=process;match=epoch;count=0;
  for(int n=0;n<size;++n)if(items[n].typeHash==hash&&nativeplacement::fresh(items[n],epoch,now,frame))offers[count++]=items[n];
  if(chosen&&!current())chosen=false;
 }
 const nativeplacement::Offer*current()const{if(chosen)for(int n=0;n<count;++n)if(nativeplacement::sameIdentity(selected,offers[n]))return &offers[n];return nullptr;}
 bool choose(int index){chosen=index>=0&&index<count;if(chosen)selected=offers[index];return chosen;}
};
// Pull this one-shot action out BEFORE generic hotkey backlog admission. A
// busy bridge rejects it at the actual input edge, never at a later cursor.
template<class Group,class Immediate>void immediate(Group&group,Immediate action){
 for(std::size_t n=0;n<group.actions.size();)if(group.actions[n]==Hotkey){action();group.actions.erase(group.actions.begin()+n);if(n<group.bindings.size())group.bindings.erase(group.bindings.begin()+n);}else ++n;
}
}}
