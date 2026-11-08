#pragma once
#include <stddef.h>
#include <stdint.h>

namespace assist { namespace techlookup {
// These tables select a captured candidate only. They never authorize a hit:
// the caller must still validate current world, identity and instruction bytes.
inline unsigned bucket(uint32_t pointer,unsigned mask){return ((pointer>>4)*2654435761u)&mask;}
template<class Identity,size_t N> const Identity* findIdentity(const Identity(&table)[N],uint32_t pointer){
 static_assert(N&&!(N&(N-1)),"identity table must have a power-of-two capacity");
 if(!pointer)return nullptr;
 unsigned n=bucket(pointer,unsigned(N-1));
 for(unsigned probe=0;probe<N;++probe,n=(n+1)&unsigned(N-1)){
  const auto&t=table[n];if(!t.pointer)return nullptr;if(t.pointer==pointer)return &t;
 }
 return nullptr;
}

// Fixed owner-thread storage: no allocation, function-local static or lazy
// initialization. A rebuild is published only after every record was indexed.
template<size_t Capacity> class PointerIndex {
 static_assert(Capacity&&!(Capacity&(Capacity-1)),"index capacity must be a power of two");
 uint8_t slots_[Capacity]{};bool ready_=false;
public:
 bool ready()const{return ready_;}
 void reset(){ready_=false;for(auto&slot:slots_)slot=0;}
 template<class Record,size_t N,class Key> void rebuild(const Record(&records)[N],Key key){
  static_assert(N<256&&N*2<=Capacity,"index must fit every record at no more than half load");
  reset();
  for(unsigned i=0;i<N;++i){
   const uint32_t pointer=key(records[i]);if(!pointer)continue;
   unsigned n=bucket(pointer,unsigned(Capacity-1));
   for(unsigned probe=0;probe<Capacity;++probe,n=(n+1)&unsigned(Capacity-1)){
    const unsigned stored=slots_[n];
    if(!stored){slots_[n]=uint8_t(i+1);break;}
    // Preserve the first matching rule, including its later identity failure.
    if(key(records[stored-1])==pointer)break;
   }
  }
  ready_=true;
 }
 template<class Record,size_t N,class Key> unsigned find(const Record(&records)[N],uint32_t pointer,Key key)const{
  static_assert(N<256&&N*2<=Capacity,"index must fit every record at no more than half load");
  if(!ready_||!pointer)return unsigned(N);
  unsigned n=bucket(pointer,unsigned(Capacity-1));
  for(unsigned probe=0;probe<Capacity;++probe,n=(n+1)&unsigned(Capacity-1)){
   const unsigned stored=slots_[n];if(!stored||stored>N)return unsigned(N);
   if(key(records[stored-1])==pointer)return stored-1;
  }
  return unsigned(N);
 }
};
}}
