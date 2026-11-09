#include "type_hotkey.h"
#include <unordered_set>
static uint32_t normalVariantWeapon(uint32_t p,uint32_t type){
 if(!get<bool>(type+0x805))return 0;
 int slot=get<int>(p+0x138),count=get<int>(type+0x80C);if(slot==-1)slot=0;
 if(slot<0||slot>=count||slot>=56||(slot>=18&&!GetModuleHandleW(L"Ares.dll")))return 1;
 // Ares extends the normal table. Its patched accessor understands that
 // extension; indexing the original 18-element table would cross into elites.
 uint32_t weapon=call<uint32_t>(0x7177C0,type,slot);
 return weapon>=0x10000?get<uint32_t>(weapon):1;
}
static int selectVariant(uint32_t house,bool global){
 if(!nativeDependenciesReady(0x48DC90,0x6FBFA0)||(!global&&!nativeDependencyReady(0x6D2140)))return -12;
 // Selection keys are not the 512-entry UI type catalogue. A modded live
 // selection can contain more variants; keep all keys in the world bound.
 static std::unordered_set<uint64_t> keys;static std::unordered_set<uint32_t> types;keys.clear();types.clear();
 Vec selected=vec(0xA8ECB8,16384);keys.reserve(size_t(selected.count));types.reserve(size_t(selected.count));
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);if(!p||!own(p,house))continue;int k=kind(p);if(k<0||k==3)continue;uint32_t t=typeOf(p,k),weapon=t?normalVariantWeapon(p,t):1;if(!t||weapon==1)continue;
  keys.insert((uint64_t(t)<<32)|weapon);types.insert(t);
 }
 if(keys.empty())return 0;uint32_t tactical=get<uint32_t>(0x887324);if(!global&&!tactical)return -4;
 static uint32_t chosen[16384];int count=0;Vec all=vec(0xA8EC78);for(int i=0;i<all.count&&count<16384;++i){uint32_t p=item(all,i);if(!p||!own(p,house))continue;int k=kind(p);if(k<0||k==3)continue;uint32_t t=typeOf(p,k);if(!t||!types.count(t))continue;
  uint32_t weapon=normalVariantWeapon(p,t);if(weapon==1||!keys.count((uint64_t(t)<<32)|weapon)||!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x13C),p))continue;
  if(!global){POINT point{};if(!call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&point))continue;}chosen[count++]=p;
 }
 if(!count)return 0;unselect();int n=0;for(int i=0;i<count;++i)if(selection(chosen[i]))++n;return n;
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
 uint32_t tactical=get<uint32_t>(0x887324);if(!global&&!tactical)return -4;
 static uint32_t chosen[16384];int n=0;Vec v=vec(0xA8EC78);
 for(int i=0;i<v.count&&n<16384;++i){uint32_t p=item(v,i);if(!own(p,house))continue;
  int k=kind(p);if(k<0||k==3||!wanted.contains(typeOf(p,k)))continue;
  if(!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x13C),p))continue;
  if(!global){POINT pt{};if(!call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&pt))continue;}
  chosen[n++]=p;
 }
 if(!n)return 0;unselect();int selected=0;for(int i=0;i<n;++i)if(selection(chosen[i]))++selected;return selected;
}
static int selectType(uint32_t house,int index,bool global){
 if(index<0||index>=snap.typeCount||index>=MaxTypes||snap.types[index].kind==7)return -4;
 uint8_t mask[MaxTypes]{};mask[index]=1;return selectTypes(house,mask,global);
}
