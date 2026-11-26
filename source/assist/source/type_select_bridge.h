#include "type_hotkey.h"
#include "variant_keys.h"
static uint32_t normalVariantWeapon(uint32_t p,uint32_t type){
 if(!get<bool>(type+0x805))return 0;
 int slot=get<int>(p+0x138),count=get<int>(type+0x80C);if(slot==-1)slot=0;
 if(slot<0||slot>=count||slot>=56||(slot>=18&&!GetModuleHandleW(L"Ares.dll")))return 1;
 // Ares extends the normal table. Its patched accessor understands that
 // extension; indexing the original 18-element table would cross into elites.
 uint32_t weapon=call<uint32_t>(0x7177C0,type,slot);
 return weapon>=0x10000?get<uint32_t>(weapon):1;
}
// One explicit selection command runs on the game's thread. Weapon identity
// is resolved by the native accessor (including Ares normal-weapon slots),
// which cannot be reconstructed from the generic async type catalogue.
// This bounded scan is never part of continuous per-frame sampling.
static int selectVariant(uint32_t house,bool global){
 if(!nativeDependenciesReady(0x48DC90,0x6FBFA0)||(!global&&!nativeDependencyReady(0x6D2140)))return -12;
 static typekeys::VariantKeys keys;keys.clear();
 Vec selected=vec(0xA8ECB8,16384);
 for(int i=0;i<selected.count;++i){
  const uint32_t p=item(selected,i);if(!p||!own(p,house))continue;
  const int k=kind(p);if(k<0||k==3)continue;
  const uint32_t id=get<uint32_t>(p+0x10),type=typeOf(p,k);
  if(!id||!type)continue;
  const uint32_t weapon=normalVariantWeapon(p,type);
  if(weapon>1)keys.add(type,weapon);
 }
 if(keys.empty())return 0;
 const uint32_t tactical=get<uint32_t>(0x887324);
 if(!global&&!tactical)return -4;
 struct Candidate{uint32_t pointer,id;};
 static Candidate chosen[16384];int count=0;
 const Vec all=vec(0xA8EC78,16384);
 for(int i=0;i<all.count&&count<16384;++i){
  const uint32_t p=item(all,i);if(!p||!own(p,house))continue;
  const int k=kind(p);if(k<0||k==3)continue;
  const uint32_t type=typeOf(p,k);if(!type||!keys.hasType(type))continue;
  const uint32_t id=get<uint32_t>(p+0x10);if(!id)continue;
  const uint32_t weapon=normalVariantWeapon(p,type);
  if(weapon<=1||!keys.contains(type,weapon))continue;
  if(!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x13C),p))continue;
  if(!global){POINT point{};if(!call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&point))continue;}
  chosen[count++]={p,id};
 }
 if(!count)return 0;
 unselect();int accepted=0;
 // Unselect and other native selection operations can affect object state.
 // A stale/reused pointer never inherits a captured candidate's identity.
 for(int i=0;i<count;++i){
  const auto &c=chosen[i];
  uint32_t currentID=0;
  if(!read(c.pointer+0x10,&currentID,sizeof currentID)||currentID!=c.id||!own(c.pointer,house))continue;
  if(selection(c.pointer))++accepted;
 }
 return accepted;
}
// Select owned, live, selectable instances of a registered mobile type.
// TacticalClass::CoordsToClient already returns whether the world coordinate
// is visible in the tactical viewport. Do not test its output against a
// (0,0)-origin rectangle: VisibleArea may have a nonzero origin.
static int selectTypes(uint32_t house,const uint8_t* mask,bool global){
 if(!nativeDependenciesReady(0x48DC90,0x6FBFA0)||(!global&&!nativeDependencyReady(0x6D2140)))return -12;
 static typekeys::TypeSet wanted;wanted.clear();int types=0;
 for(int i=0;i<MaxTypes;++i)if(mask[i]){
  if(i>=snap.typeCount||snap.types[i].kind==7||!typePointers[i])return -4;
  wanted.add(typePointers[i]);++types;
 }
 if(!types)return -4;
 static uint32_t candidates[16384],ids[16384];int count=0;
 SampleReadScope sample;if(!sample||!commandRosterReady())return -12;
 Vec all{};if(!sampleRoster(0xA8EC78,all))return -12;
 for(int i=0;i<all.count&&count<16384;++i){
  const uint32_t p=item(all,i);if(!p||!own(p,house))continue;
  const int k=kind(p);if(k<0||k==3||!wanted.contains(typeOf(p,k)))continue;
  const uint32_t id=get<uint32_t>(p+0x10);if(!id)continue;
  candidates[count]=p;ids[count++]=id;
 }
 if(!sample.complete())return -12;
 sample.finish();
 if(!count)return 0;
 const uint32_t tactical=get<uint32_t>(0x887324);if(!global&&!tactical)return -4;
 unselect();int selected=0;
 for(int i=0;i<count;++i){
  const uint32_t p=candidates[i];if(worldByID(ids[i])!=p||!own(p,house))continue;
  if(!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x13C),p))continue;
  if(!global){POINT pt{};if(!call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&pt))continue;}
  if(selection(p))++selected;
 }
 return selected;
}
static int selectType(uint32_t house,int index,bool global){
 if(index<0||index>=snap.typeCount||index>=MaxTypes||snap.types[index].kind==7)return -4;
 uint8_t mask[MaxTypes]{};mask[index]=1;return selectTypes(house,mask,global);
}
