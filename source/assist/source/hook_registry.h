#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "crc_shadow_policy.h"
#include "../../shared/source/direct_patch_policy.h"
// Game-thread-only ownership bookkeeping, NOT a stop-the-world x86 patcher.
// A stale/foreign site is sticky: never overwrite it on retries or detach.
namespace assist { namespace hookregistry {
enum class Owner : uint8_t { Frame, CS, Cursor, NativeEvent, FootClick, Decide, MouseUp, Placement, Sidebar, NativeAir, RenderMask, C4Test, PlayerControl, Camera, Combat, NetworkTransport, NativeSell, NativeMega, ControlCommand, CrcShadow, Unknown };
inline Owner ownerAt(uint32_t a){
 if(crcshadow::findSite(a))return Owner::CrcShadow;
 if(auto s=directpatch::find(a))return s->group==directpatch::Group::Map?Owner::RenderMask:Owner::Placement;
 switch(a){
 case 0x007F74D8:return Owner::Sidebar;
 case 0x00646EF5:return Owner::ControlCommand;
 case 0x00646FD2:return Owner::ControlCommand;
 case 0x006FFC93:return Owner::ControlCommand;
 case 0x006FFE2B:return Owner::ControlCommand;

 case 0x00649CA0:return Owner::NetworkTransport;
 case 0x00649E1E:return Owner::NetworkTransport;
 case 0x0064B9D4:return Owner::NetworkTransport;
 case 0x0064B660:return Owner::NetworkTransport;
 case 0x0064B4EA:return Owner::NetworkTransport;
 case 0x00649D61:case 0x00649DBC:return Owner::NetworkTransport;
 case 0x006929E5:return Owner::NativeSell;
 case 0x004AC179:return Owner::NativeSell;
 case 0x00646F4E:return Owner::NativeMega;
 case 0x006F7D44:return Owner::Combat;
 case 0x0055DE81:case 0x0055D360: return Owner::Frame;
 case 0x00646E90: return Owner::CS;
 case 0x00731BF0: return Owner::Cursor;
 case 0x006FFE00: return Owner::NativeEvent;
 case 0x004D74E0: return Owner::FootClick;
 case 0x00692610: return Owner::Decide;
 case 0x004AB9B0: return Owner::MouseUp;
 case 0x00464AC0:case 0x004A8EB0: return Owner::Placement;
 case 0x007E2908:case 0x00416AB6:return Owner::NativeAir;
 case 0x0047F038:case 0x0047F274:return Owner::RenderMask;
 case 0x0051A4D8:case 0x0051C32B:return Owner::C4Test;
 case 0x006D8640:case 0x004A9840:return Owner::Camera;
 case 0x44574A:case 0x445795:case 0x4457D0:case 0x445810:case 0x6AA788:
 case 0x006A97D2:case 0x006AA781:return Owner::Sidebar;
 case 0x50B6F0:case 0x6339BA:case 0x637E40:return Owner::PlayerControl;
 // Six verified CALL-sites use the same frame owner.
 case 0x0048CE8A:case 0x00608206:case 0x0060D407:
 case 0x0062314E:case 0x00623D72:case 0x00624271: return Owner::Frame;
 default:return Owner::Unknown;
 }
}
inline bool knownOriginal(uint32_t at,const uint8_t* bytes,unsigned size){
 if(!bytes)return false;
 if(auto s=crcshadow::findSite(at))return size==s->size&&!memcmp(bytes,s->original,size);
 if(directpatch::find(at))return directpatch::original(at,bytes,size);
 const uint8_t*signature=nullptr;unsigned expected=0;
 static const uint8_t pre6NetworkSend[]={0x83,0xEC,0x1C,0x53,0x55};
 static const uint8_t pre6NetworkPlainHouse[]={0x88,0x8C,0x40,0xD6,0x02,0xA8,0x00};
 static const uint8_t pre6NetworkCompressedHouse[]={0x88,0x8C,0x40,0xD6,0x02,0xA8,0x00};
 static const uint8_t pre6NetworkPack[]={0x81,0xEC,0x94,0x00,0x00,0x00};
 static const uint8_t pre6NetworkDictionary[]={0x8B,0xCA,0xC7,0x44,0x24,0x10,0x00,0x00,0x00,0x00};
 static const uint8_t houseFrameGroup[]={0xA1,0x4C,0xB2,0xA8,0x00};
 static const uint8_t housePlainLimit[]={0x8B,0x54,0x24,0x14,0x8B,0x74,0x24,0x10};
 static const uint8_t pre6NativeSellCursor[]={0xE8,0x06,0x8D,0xE7,0xFF};
 static const uint8_t pre6NativeSellOwner[]={0x8B,0x0D,0x4C,0x3D,0xA8,0x00,0x8B,0x51,0x30};
 static const uint8_t pre6NativeMegaSuccess[]={0xB0,0x01,0x5F,0x5E,0x83,0xC4,0x70};
 static const uint8_t pre6ControlMegaOwner[]={0x81,0x3D,0xC8,0x02,0xA8,0x00,0x80,0x00,0x00,0x00};
 static const uint8_t pre6ControlMegaFOwner[]={0x81,0x3D,0xC8,0x02,0xA8,0x00,0x80,0x00,0x00,0x00};
 static const uint8_t pre6ControlPlanningOwner[]={0x83,0xEC,0x70,0xB9,0x1B,0x00,0x00,0x00};
 static const uint8_t pre6ControlObjectOwner[]={0x8B,0x42,0x30,0x51,0x50};
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
 static const uint8_t lowDamage[]={0xD8,0x1D,0x38,0x4E,0x7F,0};
 static const uint8_t scrolling[]={0x8B,0x44,0x24,0x08,0x8A,0x4C,0x24,0x0C};
 static const uint8_t camera[]={0xA1,0xE4,0xF8,0x87,0};
 static const uint8_t controlEntries[3][5]={{0xA1,0x38,0xB2,0xA8,0},{0xE9,0xFB,0x2A,0x67,0},{0xE9,0x8B,0x2C,0x67,0}};
 static const uint8_t controlOwner[]={0xE8,0x31,0x7D,0xED,0xFF},controlPlanning[]={0xE8,0xAB,0x38,0xED,0xFF};
 static const uint8_t sidebarClick[]={0xE8,0x99,0xE0,0xE4,0xFF};
 static const uint8_t sidebarRefresh[]={0xE8,0xEA,0xD0,0xE4,0xFF};
 static const uint8_t masterEnum[]={0x6A,1,0x6A,0,0x8B},masterPrune[]={0x0F,0x85,0xDA,2,0,0};
 switch(at){
 case 0x007F74D8:{static const uint8_t original[]={0x50,0x90,0x47,0};return size==4&&!memcmp(bytes,original,4);}
 case 0x00646EF5:signature=pre6ControlMegaOwner;expected=sizeof pre6ControlMegaOwner;break;
 case 0x00646FD2:signature=pre6ControlMegaFOwner;expected=sizeof pre6ControlMegaFOwner;break;
 case 0x006FFC93:signature=pre6ControlPlanningOwner;expected=sizeof pre6ControlPlanningOwner;break;
 case 0x006FFE2B:signature=pre6ControlObjectOwner;expected=sizeof pre6ControlObjectOwner;break;

 case 0x00649CA0:signature=pre6NetworkSend;expected=sizeof pre6NetworkSend;break;
 case 0x00649E1E:signature=pre6NetworkPlainHouse;expected=sizeof pre6NetworkPlainHouse;break;
 case 0x0064B9D4:signature=pre6NetworkCompressedHouse;expected=sizeof pre6NetworkCompressedHouse;break;
 case 0x0064B660:signature=pre6NetworkPack;expected=sizeof pre6NetworkPack;break;
 case 0x0064B4EA:signature=pre6NetworkDictionary;expected=sizeof pre6NetworkDictionary;break;
 case 0x00649D61:signature=houseFrameGroup;expected=sizeof houseFrameGroup;break;
 case 0x00649DBC:signature=housePlainLimit;expected=sizeof housePlainLimit;break;
 case 0x006929E5:signature=pre6NativeSellCursor;expected=sizeof pre6NativeSellCursor;break;
 case 0x004AC179:signature=pre6NativeSellOwner;expected=sizeof pre6NativeSellOwner;break;
 case 0x00646F4E:signature=pre6NativeMegaSuccess;expected=sizeof pre6NativeMegaSuccess;break;
 case 0x006F7D44:signature=lowDamage;expected=sizeof lowDamage;break;
 case 0x44574A:case 0x445795:case 0x4457D0:case 0x445810:signature=masterEnum;expected=5;break;
 case 0x6AA788:signature=masterPrune;expected=6;break;
 case 0x0055DE81:{static const uint8_t committed[]={0x89,0x15,0x84,0xED,0xA8,0};return size==6&&!memcmp(bytes,committed,6);}
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
 case 0x0051A4D8:case 0x0051C32B:signature=c4;expected=sizeof c4;break;
 case 0x004A9840:signature=scrolling;expected=sizeof scrolling;break;
 case 0x006D8640:signature=camera;expected=sizeof camera;break;
 case 0x50B6F0:return size==5&&(!memcmp(bytes,controlEntries[0],5)||!memcmp(bytes,controlEntries[1],5)||!memcmp(bytes,controlEntries[2],5));
 case 0x6339BA:signature=controlOwner;expected=5;break;
 case 0x637E40:signature=controlPlanning;expected=5;break;
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
template<unsigned Capacity=64> struct Registry {
 static constexpr unsigned CapacityLimit=Capacity;
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

