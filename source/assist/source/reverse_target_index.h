#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace assist { namespace reversedodge {
// A retained flat index of one sealed roster. Linked bucket tails preserve
// original roster order without per-actor allocation or sorting on the game.
class TargetIndex {
 static constexpr uint32_t Capacity=16384,Buckets=32768;
 struct Entry {uint32_t pointer=0,id=0,next=0;};
 std::array<Entry,Capacity>entries_{};
 std::array<uint32_t,Buckets>cells_{},heads_{},tails_{};
 uint32_t count_=0;
 static uint32_t hash(uint32_t value){value^=value>>16;value*=0x7feb352du;return (value^(value>>15))&(Buckets-1);}
 uint32_t slot(uint32_t cell)const{uint32_t at=hash(cell);while(heads_[at]&&cells_[at]!=cell)at=(at+1)&(Buckets-1);return at;}
public:
 void clear(){count_=0;heads_.fill(0);}
 bool add(uint32_t cell,uint32_t pointer,uint32_t id){
  if(count_==Capacity)return false;const uint32_t at=slot(cell),number=++count_;
  entries_[number-1]={pointer,id,0};
  if(heads_[at])entries_[tails_[at]-1].next=number;else{cells_[at]=cell;heads_[at]=number;}
  tails_[at]=number;return true;
 }
 template<class Visit>void visit(uint32_t cell,Visit visit)const{
  for(uint32_t at=heads_[slot(cell)];at;at=entries_[at-1].next){const auto&e=entries_[at-1];visit(e.pointer,e.id);}
 }
};
}}
