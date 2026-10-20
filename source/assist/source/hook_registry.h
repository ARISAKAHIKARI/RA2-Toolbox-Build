#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// Game-thread-only ownership bookkeeping, NOT a stop-the-world x86 patcher.
// A stale/foreign site is sticky: never overwrite it on retries or detach.
namespace assist { namespace hookregistry {
enum class Owner : uint8_t { Frame, CS, Cursor, NativeEvent, FootClick, Decide, MouseUp, Placement, Sidebar, NativeAir, RenderMask, C4Test, PlayerSelection, Unknown };
inline Owner ownerAt(uint32_t a){
 switch(a){
 case 0x0055D360: return Owner::Frame;
 case 0x00646E90: return Owner::CS;
 case 0x00731BF0: return Owner::Cursor;
 case 0x006FFE00: return Owner::NativeEvent;
 case 0x004D74E0: return Owner::FootClick;
 case 0x00692610: return Owner::Decide;
 case 0x004AB9B0: return Owner::MouseUp;
 case 0x00464AC0:case 0x004A8EB0: return Owner::Placement;
 case 0x007E2908:case 0x00416AB6:return Owner::NativeAir;
 case 0x0047F038:case 0x0047F274:return Owner::RenderMask;
 case 0x0051A4D8:return Owner::C4Test;
 case 0x006A97D2:case 0x006AA781:return Owner::Sidebar;
 case 0x6FBFB3:case 0x6FBFEF:case 0x5F45D2:case 0x5F45DF:case 0x5F45EF:return Owner::PlayerSelection;
 // Six verified CALL-sites use the same frame owner.
 case 0x0048CE8A:case 0x00608206:case 0x0060D407:
 case 0x0062314E:case 0x00623D72:case 0x00624271: return Owner::Frame;
 default:return Owner::Unknown;
 }
}
inline bool knownOriginal(uint32_t at,const uint8_t* bytes,unsigned size){
 if(!bytes)return false;
 const uint8_t*signature=nullptr;unsigned expected=0;
 static const uint8_t frame[]={0xA0,0xA0,0xE9,0xA8,0x00};
 static const uint8_t cs[]={0x83,0xEC,0x70,0x32,0xC0};
 static const uint8_t cursor[]={0x51,0xA0,0x58,0xFE,0xB0,0x00};
 static const uint8_t input[]={0x83,0xEC,0x78,0x56,0x57};
 static const uint8_t foot[]={0x81,0xEC,0xF8,0,0,0};
 static const uint8_t decide[]={0x83,0xEC,0x10,0xA1,0xC8,0xEC,0xA8,0};
 static const uint8_t mouse[]={0x83,0xEC,0x7C,0x53,0x55,0x56};
 static const uint8_t place[]={0x8A,0x81,0x03,0x17,0,0};
 static const uint8_t proximity[]={0xA1,0x4C,0x3D,0xA8,0};
 static const uint8_t airType[]={0x50,0xCB,0x41,0};
 static const uint8_t airArrival[]={0xC7,0x86,0xBC,0,0,0,3,0,0,0};
 static const uint8_t shroud[]={0x8B,0x45,0x10,0x53,0x56};
 static const uint8_t fog[]={0x8B,0x44,0x24,0x38,0x53};
 static const uint8_t c4[]={0x8A,0x81,0xC2,0x0E,0,0};
 static const uint8_t selectionCalls[5][5]={{0xE8,0x38,0xF7,0xE0,0xFF},{0xE8,0xFC,0xF6,0xE0,0xFF},{0xE8,0x19,0x71,0xF1,0xFF},{0xE8,0x0C,0x71,0xF1,0xFF},{0xE8,0xFC,0x70,0xF1,0xFF}};
 static const uint8_t sidebarClick[]={0xE8,0x99,0xE0,0xE4,0xFF};
 static const uint8_t sidebarRefresh[]={0xE8,0xEA,0xD0,0xE4,0xFF};
 switch(at){
 case 0x0055D360:signature=frame;expected=sizeof frame;break;
 case 0x00646E90:signature=cs;expected=sizeof cs;break;
 case 0x00731BF0:signature=cursor;expected=sizeof cursor;break;
 case 0x006FFE00:signature=input;expected=sizeof input;break;
 case 0x004D74E0:signature=foot;expected=sizeof foot;break;
 case 0x00692610:signature=decide;expected=sizeof decide;break;
 case 0x004AB9B0:signature=mouse;expected=sizeof mouse;break;
 case 0x00464AC0:signature=place;expected=sizeof place;break;
 case 0x004A8EB0:signature=proximity;expected=sizeof proximity;break;
 case 0x007E2908:signature=airType;expected=sizeof airType;break;
 case 0x00416AB6:signature=airArrival;expected=sizeof airArrival;break;
 case 0x0047F038:signature=shroud;expected=sizeof shroud;break;
 case 0x0047F274:signature=fog;expected=sizeof fog;break;
 case 0x0051A4D8:signature=c4;expected=sizeof c4;break;
 case 0x6FBFB3:signature=selectionCalls[0];expected=5;break;
 case 0x6FBFEF:signature=selectionCalls[1];expected=5;break;
 case 0x5F45D2:signature=selectionCalls[2];expected=5;break;
 case 0x5F45DF:signature=selectionCalls[3];expected=5;break;
 case 0x5F45EF:signature=selectionCalls[4];expected=5;break;
 case 0x006A97D2:signature=sidebarClick;expected=sizeof sidebarClick;break;
 case 0x006AA781:signature=sidebarRefresh;expected=sizeof sidebarRefresh;break;
 case 0x0048CE8A:case 0x00608206:case 0x0060D407:
 case 0x0062314E:case 0x00623D72:case 0x00624271:
  return size==5&&bytes[0]==0xE8; // caller separately checks 21-byte EXE context and call target
 default:return false;
 }
 return size==expected&&!memcmp(bytes,signature,size);
}
struct Slot {
 uint32_t address=0;Owner owner=Owner::Unknown;uint8_t original[16]{},patch[16]{};
 uint8_t size=0;bool active=false,conflict=false;
};
enum class Transition : uint8_t { Deny, Install, Restore };
template<unsigned Capacity=16> struct Registry {
 Slot slots[Capacity]{};unsigned used=0;
 int find(uint32_t a)const{for(unsigned i=0;i<used;++i)if(slots[i].address==a)return int(i);return -1;}
 bool conflict(uint32_t a)const{int i=find(a);return i>=0&&slots[i].conflict;}
 bool active(uint32_t a)const{int i=find(a);return i>=0&&slots[i].active;}
 void markConflict(uint32_t a){
  int i=find(a);
  if(i<0&&ownerAt(a)!=Owner::Unknown&&used<Capacity){
   i=int(used++);slots[i]={};slots[i].address=a;slots[i].owner=ownerAt(a);
  }
  if(i>=0)slots[i].conflict=true;
 }
 unsigned conflicts()const{unsigned n=0;for(unsigned i=0;i<used;++i)if(slots[i].conflict)++n;return n;}
 unsigned installed()const{unsigned n=0;for(unsigned i=0;i<used;++i)if(slots[i].active)++n;return n;}
 Transition proposal(uint32_t address,const uint8_t*before,const uint8_t*after,unsigned n)const{
  if(!before||!after||!n||n>16||address<0x10000||ownerAt(address)==Owner::Unknown||!memcmp(before,after,n))return Transition::Deny;
  int i=find(address);if(i<0)return used<Capacity&&knownOriginal(address,before,n)?Transition::Install:Transition::Deny;
  const Slot&s=slots[i];if(s.conflict||s.owner!=ownerAt(address)||s.size!=n)return Transition::Deny;
  if(s.active)return !memcmp(before,s.patch,n)&&!memcmp(after,s.original,n)?Transition::Restore:Transition::Deny;
  // A cleanly restored site may receive a fresh immutable trampoline.
  // The manager still requires the same owning module and live lease.
  return !memcmp(before,s.original,n)?Transition::Install:Transition::Deny;
 }
 void commit(uint32_t address,const uint8_t*before,const uint8_t*after,unsigned n,Transition t){
  if(t==Transition::Install){int i=find(address);
   if(i<0){i=int(used++);slots[i]={};slots[i].address=address;slots[i].owner=ownerAt(address);slots[i].size=uint8_t(n);
    memcpy(slots[i].original,before,n);memcpy(slots[i].patch,after,n);}
   memcpy(slots[i].patch,after,n);slots[i].active=true;
  }else if(t==Transition::Restore){int i=find(address);if(i>=0)slots[i].active=false;}
 }
 // No reset: a foreign patch survives match changes and failed restores.
};
}}
