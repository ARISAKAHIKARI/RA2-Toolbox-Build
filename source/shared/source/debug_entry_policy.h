#pragma once
#include <string.h>
#include "debug_entry_types.h"
#include "debug_entry_catalog.h"
namespace debugentries {
static_assert(Count<=Capacity,"debug entry catalogue exceeds transport capacity");
// Entry identities and their order are immutable. Build their lookup once at
// compile time; policy and observations remain live and are never cached here.
inline constexpr unsigned IndexBits=9;
inline constexpr uint32_t IndexBuckets=1u<<IndexBits;
static_assert(Count<IndexBuckets&&Count<=0xFFFFu,"debug entry index exceeds its fixed storage");
inline constexpr uint32_t indexBucket(uint32_t key){return ((key^(key>>16))*2654435761u)>>(32-IndexBits);}
struct CatalogueIndex {
 uint16_t slots[IndexBuckets]{};
 constexpr CatalogueIndex(){
  for(uint32_t i=0;i<Count;++i){uint32_t slot=indexBucket(Entries[i].id);
   while(slots[slot]&&Entries[slots[slot]-1].id!=Entries[i].id)slot=(slot+1)&(IndexBuckets-1);
   if(!slots[slot])slots[slot]=uint16_t(i+1);
  }
 }
};
inline constexpr CatalogueIndex catalogueIndex;
inline int index(uint32_t key){
 uint32_t slot=indexBucket(key);
 for(uint32_t probe=0;probe<IndexBuckets;++probe,slot=(slot+1)&(IndexBuckets-1)){
  const uint16_t entry=catalogueIndex.slots[slot];if(!entry)return -1;
  if(Entries[entry-1].id==key)return int(entry-1);
 }return -1;
}
inline const Entry*find(uint32_t key){const int i=index(key);return i<0?nullptr:&Entries[i];}
inline bool required(const Config&c,uint32_t key){
 const auto*e=find(key);if(!e||!(e->flags&Checkable))return true;
 for(uint32_t i=0;i<c.count&&i<Capacity;++i)if(c.skip[i]==key)return false;
 return true;
}
inline bool setSkip(Config&c,uint32_t key,bool skip){
 const auto*e=find(key);if(!e||!(e->flags&Checkable)||c.count>Capacity)return false;
 uint32_t i=0;while(i<c.count&&c.skip[i]!=key)++i;
 if(skip){if(i<c.count||c.count==Capacity)return false;c.skip[c.count++]=key;}
 else{if(i==c.count)return false;for(++i;i<c.count;++i)c.skip[i-1]=c.skip[i];c.skip[--c.count]=0;}
 if(!++c.generation)c.generation=1;return true;
}
// One instance across the linked GameRuntime translation units. Only the
// leased game thread changes policy/observations; the host receives a seqlock copy.
inline Config activePolicy;
inline uint32_t policyRevision=1,clockTick=0;
inline bool(*executableProbe)(uint32_t,size_t)=nullptr; // Bound by the leased Runtime before feature admission.
inline Observation observations[Capacity]{};
inline bool applyPolicy(const Config&source){
 if(!source.generation||source.count>Capacity)return false;
 Config next{};next.generation=source.generation;
 for(uint32_t i=0;i<source.count;++i){const auto*e=find(source.skip[i]);if(!e||!(e->flags&Checkable))continue;
  bool duplicate=false;for(uint32_t j=0;j<next.count;++j)if(next.skip[j]==e->id)duplicate=true;
  if(!duplicate)next.skip[next.count++]=e->id;
 }
 if(!memcmp(&next,&activePolicy,sizeof next))return false;
 for(uint32_t i=0;i<Count;++i)if(required(activePolicy,Entries[i].id)!=required(next,Entries[i].id)){
  auto&o=observations[i];o.signature=Signature::Unknown;o.policyGeneration=0;
  if(o.reason==Reason::Signature||o.reason==Reason::Unreadable)o.reason=Reason::AwaitRuntime;
 }
 activePolicy=next;if(!++policyRevision)policyRevision=1;return true;
}
inline bool signatureRequired(Kind kind,uint32_t address){return address>=0x01000000u||required(activePolicy,id(kind,address));}
inline Observation*observation(Kind kind,uint32_t address){
 if(address>=0x01000000u)return nullptr;const int i=index(id(kind,address));if(i<0)return nullptr;auto&o=observations[i];
 if(!o.id){o.id=Entries[i].id;o.hook=kind==Kind::Call?Hook::NA:Hook::Inactive;}
 return &o;
}
// Readability/executability and current ownership are separate from the
// replaceable reference-pattern decision. Callers must supply a real read.
inline bool allow(Kind kind,uint32_t address,bool readable,bool matched,Signature success=Signature::Match){
 const bool bypass=!signatureRequired(kind,address);
 if(bypass&&kind==Kind::Call&&readable&&executableProbe)readable=executableProbe(address,1);
 if(auto*o=observation(kind,address)){
  o->signature=!readable?Signature::Unreadable:bypass?Signature::Skipped:matched?success:Signature::Mismatch;
  o->tick=clockTick;o->policyGeneration=activePolicy.generation;
  if(!readable)o->reason=Reason::Unreadable;
  else if(!bypass&&!matched)o->reason=Reason::Signature;
  else if(o->reason==Reason::Signature||o->reason==Reason::Unreadable||o->reason==Reason::AwaitRuntime)o->reason=Reason::None;
 }
 return readable&&(bypass||matched);
}
inline void installation(Kind kind,uint32_t address,Hook hook,Reason reason=Reason::None,int error=0){
 if(auto*o=observation(kind,address)){o->hook=hook;o->reason=reason;o->error=error;o->tick=clockTick;o->policyGeneration=activePolicy.generation;}
}
inline void snapshot(Snapshot&out,uint32_t now){
 out.policyGeneration=activePolicy.generation;out.tick=now;out.count=Count;
 for(uint32_t i=0;i<Count;++i){out.entries[i]=observations[i];if(!out.entries[i].id){out.entries[i].id=Entries[i].id;
   out.entries[i].hook=Entries[i].kind==Kind::Call?Hook::NA:Hook::Inactive;
   out.entries[i].reason=(Entries[i].flags&Disabled)?Reason::Disabled:Reason::NoRequest;
  }}
}
}
