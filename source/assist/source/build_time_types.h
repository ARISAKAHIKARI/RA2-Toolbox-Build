#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace assist { namespace buildtime {
// YRpp d979554 / Phobos f06797e; supported native image 268a81b7...fbb8d0.
// These are four different receiver layouts, not a Building with another Type.
struct TypeInfo {
 int cameoKind,objectKind,nativeKind;
 uint32_t array,vtable,typeVtable,typeOffset,getter,what,typeWhat;
};
inline constexpr TypeInfo Types[]={
 {7,3,6,0xA83C68,0x7E3EBC,0x7E4570,0x520,0x459EE0,0x459EC0,0x465D90},
 {40,2,1,0xA83CE0,0x7F5C70,0x7F6218,0x6C4,0x741490,0x746E20,0x748170},
 {16,1,15,0xA8E348,0x7EB058,0x7EB610,0x6C0,0x51FAF0,0x523340,0x524D40},
 {3,0,2,0xA8B218,0x7E22A4,0x7E2868,0x6C4,0x41C200,0x41C180,0x41CFB0}
};
inline constexpr size_t ReceiverBytes=0x6C8;
static_assert(Types[0].typeOffset+4<=ReceiverBytes&&Types[1].typeOffset+4<=ReceiverBytes&&
 Types[2].typeOffset+4<=ReceiverBytes&&Types[3].typeOffset+4<=ReceiverBytes,"native query receiver prefix");
inline const TypeInfo* typeInfo(int cameoKind){for(const auto&t:Types)if(t.cameoKind==cameoKind)return &t;return nullptr;}
template<class Read>bool wordEquals(uint32_t at,uint32_t expected,Read read){uint32_t value=0;return read(at,&value,sizeof value)&&value==expected;}
template<class Read>bool codeEquals(uint32_t at,const uint8_t*expected,size_t n,Read read){
 uint8_t bytes[8]{};return n<=sizeof bytes&&read(at,bytes,n)&&!memcmp(bytes,expected,n);
}
template<class Read>bool codeHash(uint32_t at,size_t n,uint32_t expected,Read read){
 uint8_t bytes[438]{};if(!n||n>sizeof bytes||!read(at,bytes,n))return false;
 uint32_t hash=2166136261u;for(size_t i=0;i<n;++i)hash=(hash^bytes[i])*16777619u;return hash==expected;
}
template<class Read>bool receiverCompatible(const TypeInfo&t,uint32_t type,Read read){
 // The complete native query was executed for all four kinds. It only reads
 // vtable, Owner+21C and the kind's Type field from this 6C8-byte prefix.
 // Full instance sizes (720/8E8/6F0/6D8) are deliberately not constructed.
 // This is a structural safety gate for a partial receiver, not an admission
 // of unknown detours through the debug reference-comparison override.
 if(!wordEquals(type,t.typeVtable,read)||!wordEquals(t.vtable+0x84,0x6F3270,read)||
    !wordEquals(t.vtable+0x88,t.getter,read)||!wordEquals(t.vtable+0x2C,t.what,read)||
    !wordEquals(t.typeVtable+0x88,0x711EE0,read)||!wordEquals(t.typeVtable+0x2C,t.typeWhat,read))return false;
 const uint8_t forward[]={0x8B,0x01,0xFF,0xA0,0x88,0,0,0};
 const uint8_t getter[]={0x8B,0x81,uint8_t(t.typeOffset),uint8_t(t.typeOffset>>8),0,0,0xC3};
 const uint8_t what[]={0xB8,uint8_t(t.nativeKind),0,0,0,0xC3};
 const uint8_t typeWhat[]={0xB8,uint8_t(t.cameoKind),0,0,0,0xC3};
 return codeEquals(0x6F3270,forward,sizeof forward,read)&&codeEquals(t.getter,getter,sizeof getter,read)&&
  codeEquals(t.what,what,sizeof what,read)&&codeEquals(t.typeWhat,typeWhat,sizeof typeWhat,read)&&
  codeHash(0x6F47A0,438,0x96813134u,read)&&codeHash(0x711EE0,28,0x3EFFBCE2u,read);
}
}}
