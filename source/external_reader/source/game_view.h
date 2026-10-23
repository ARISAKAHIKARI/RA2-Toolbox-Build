#pragma once
#include "../../assist/source/protocol.h"
#include "../../assist/source/autoload.h"
#include "../../assist/source/weapon_policy.h"
#include <vector>
#include <cmath>
#include <cstring>
namespace external {
struct EntryCapacity {int maximum=0,used=0;double limit=0;bool occupants=false,singleVehicle=false;};
template<class Read> class GameView {
 Read read_;
public:
 explicit constexpr GameView(Read r):read_(r){}
 template<class T>T get(uint32_t a,T fallback={})const{T v{};return a>=0x10000&&read_(a,&v,sizeof v)?v:fallback;}
 bool bytes(uint32_t a,void*p,size_t n)const{return a>=0x10000&&read_(a,p,n);}
 std::vector<uint32_t> vec(uint32_t a,int maximum=16384)const{
  uint32_t p=get<uint32_t>(a+4);int cap=get<int>(a+8,-1),n=get<int>(a+16,-1);
  if(n<0||n>maximum||cap<n||cap>65536||(n&&p<0x10000))return {};
  std::vector<uint32_t> out(size_t(n),0);if(n&&!bytes(p,out.data(),size_t(n)*4))out.clear();return out;
 }
 int kind(uint32_t p)const{uint32_t v=get<uint32_t>(p);return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:v==0x7E22A4?0:-1;}
 uint32_t type(uint32_t p,int k)const{return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}
 bool live(uint32_t p)const{return p>=0x10000&&kind(p)>=0&&get<int>(p+0x6C)>0&&get<uint8_t>(p+0x74)&&!get<uint8_t>(p+0x81)&&get<uint8_t>(p+0x90);}
 bool neutral(uint32_t house)const{uint32_t country=get<uint32_t>(house+0x34);return country>=0x10000&&get<uint8_t>(country+0x1A6);}
 bool allied(uint32_t owner,uint32_t house)const{if(owner==house)return true;int i=get<int>(owner+0x30,-1);return i>=0&&i<32&&(get<uint32_t>(house+0x5788)&(1u<<i));}
 bool enemy(uint32_t p,uint32_t house)const{uint32_t owner=get<uint32_t>(p+0x21C);return live(p)&&owner>=0x10000&&!neutral(owner)&&!allied(owner,house);}
 // RadioClass stores allocated links at E4/E8, including null slots. Counting
 // E8 as occupancy makes an unused depot/bunker look full. Match the native
 // contact scan (0x65AE30); malformed/unreadable storage stays unavailable.
 int radioContacts(uint32_t p)const{
  int n=get<int>(p+0xE8,-1);uint32_t items=get<uint32_t>(p+0xE4);if(n<0||n>64||(n&&items<0x10000))return 1024;
  int used=0;for(int i=0;i<n;++i)if(get<uint32_t>(items+uint32_t(i)*4,0xffffffffu))++used;return used;
 }
 bool radioLinked(uint32_t p,uint32_t source)const{
  int n=get<int>(p+0xE8,-1);uint32_t items=get<uint32_t>(p+0xE4);if(n<0||n>64||(n&&items<0x10000))return false;
  for(int i=0;i<n;++i)if(get<uint32_t>(items+uint32_t(i)*4)==source)return true;return false;
 }
 EntryCapacity capacity(uint32_t p)const{
  EntryCapacity out;int k=kind(p);uint32_t t=type(p,k);if(!live(p)||!t||get<uint8_t>(p+0x1C8))return out;
  if(k==3&&get<uint8_t>(t+0x157B)){out.maximum=get<int>(t+0x1580);out.used=get<int>(p+0x694);out.limit=128;out.occupants=true;}
  else if(k==3&&(get<uint8_t>(t+0x16AB)||get<uint8_t>(t+0x16A9))){
   bool bunker=get<uint8_t>(t+0x16AB)!=0;int docks=get<int>(t+0x1780,1);
   if(!bunker&&(docks<0||docks>64))return {};
   out.maximum=bunker?1:(docks>0?docks:1);out.used=radioContacts(p);
   if((get<uint32_t>(p+0x2E4)||get<int>(p+0x114)!=0)&&out.used<1)out.used=1;
   out.limit=128;out.singleVehicle=true;
  }
  else if(k==2||(k==3&&(get<uint8_t>(t+0x16AE)||get<uint8_t>(t+0x16AF)))){
   out.maximum=get<int>(t+0x5E0);out.limit=get<double>(t+0x388);int count=get<int>(p+0x114,-1);uint32_t q=get<uint32_t>(p+0x118),seen[256]{};int n=0;
   if(count<0||count>256)return {};
   while(q>=0x10000&&n<256){for(int j=0;j<n;++j)if(seen[j]==q)return {};seen[n++]=q;uint32_t qt=type(q,kind(q));int size=qt?assist::loading::slots(get<double>(qt+0x380)):0;if(!size)return {};out.used+=size;q=get<uint32_t>(q+0x30);}
   if(n!=count||q)return {};
  }
  if(out.maximum<=0||out.maximum>1024||out.used<0||!(out.limit>0&&out.limit<=128))return {};
  return out;
 }
 int entrySlots(uint32_t source,uint32_t target)const{auto c=capacity(target);if(!c.maximum)return 0;if(c.occupants||c.singleVehicle)return 1;uint32_t t=type(source,kind(source));return t?assist::loading::slots(get<double>(t+0x380)):0;}
 uint32_t entryDestination(uint32_t source)const{
  if(get<uint32_t>(source+0x11C)||get<uint32_t>(source+0x2E4))return 0;
  int mission=get<int>(source+0xAC,-1),queued=get<int>(source+0xB4,-1);bool entering=mission==7||queued==7,repairing=mission==9||queued==9;
  bool moving=mission==2||queued==2;if(!entering&&!repairing&&!moving)return 0;
  uint32_t target=get<uint32_t>(source+0x500);if(target<0x10000)target=get<uint32_t>(source+0x5A4);if(target<0x10000)target=get<uint32_t>(source+0x2B4);
  if(!entering){if(kind(target)!=3)return 0;uint32_t t=type(target,3);if(!(repairing&&get<uint8_t>(t+0x16A9))&&!(moving&&get<uint8_t>(t+0x16AB)))return 0;}
  return target;
 }
 bool entered(uint32_t source,uint32_t target)const{
  if(get<uint32_t>(source+0x11C)==target)return true;
  return kind(target)==3&&get<uint8_t>(type(target,3)+0x16AB)&&get<uint32_t>(source+0x2E4)==target&&get<uint32_t>(target+0x2E4)==source;
 }
 bool entryCompatible(uint32_t source,uint32_t target,uint32_t house)const{
  int sk=kind(source),tk=kind(target);uint32_t st=type(source,sk),tt=type(target,tk),owner=get<uint32_t>(target+0x21C);
  if(source==target||!live(source)||!live(target)||!st||!tt||(sk!=1&&sk!=2)||get<uint8_t>(source+0x1C8)||get<uint32_t>(source+0x11C)||get<uint32_t>(source+0x2E4)||get<uint32_t>(source+0x21C)!=house)return false;
  if(!allied(owner,house)&&!(tk==3&&neutral(owner)&&get<uint8_t>(tt+0x157B)))return false;
  if(get<uint8_t>(source+0x134)||get<int>(source+0x94)==3||get<uint8_t>(source+0x84)||get<uint8_t>(source+0x8D))return false;
  auto c=capacity(target);int slots=entrySlots(source,target);if(!c.maximum||!slots||slots>c.limit)return false;
  if(tk==3){if(c.occupants)return sk==1&&get<uint8_t>(st+0xEB4);
   if(get<uint8_t>(tt+0x16AB))return sk==2&&get<uint8_t>(st+0xD2E)&&!get<uint32_t>(target+0x2E4)&&!get<int>(target+0x114); // Empty bunker; radio contact alone is only a reservation.
   if(get<uint8_t>(tt+0x16A9))return sk==2;
   return sk==1?get<uint8_t>(tt+0x16AF)!=0:sk==2&&get<uint8_t>(tt+0x16AE)!=0;
  }return tk==2;
 }
 assist::planner::Weapon weapon(uint32_t p)const{
  using namespace assist::planner;Weapon w{};if(p<0x10000||get<uint8_t>(p+0x136))return w;
  w.range=get<int>(p+0xB4);w.minimum=get<int>(p+0xB8);if(w.range<=0||w.range>=65536||w.minimum<0||w.minimum>w.range)return {};
  uint32_t projectile=get<uint32_t>(p+0xA0),warhead=get<uint32_t>(p+0xAC);if(projectile<0x10000||warhead<0x10000)return {};
  if(get<uint8_t>(projectile+0x2A4))w.flags|=AirWeapon;if(get<uint8_t>(projectile+0x2A5))w.flags|=GroundWeapon;
  if(get<int>(p+0xA4)<0)w.flags|=HealingWeapon;if(get<uint8_t>(warhead+0x155))w.flags|=MindControlWeapon;if(get<uint8_t>(warhead+0x178))w.flags|=PsychicWeapon;
  if(get<uint8_t>(warhead+0x15A))w.flags|=TemporalWeapon;if(get<uint8_t>(warhead+0x159))w.flags|=ParasiteWeapon;
  if(get<uint8_t>(p+0x15C))w.flags|=MagneticWeapon;if(get<uint8_t>(warhead+0x15B))w.flags|=LocomotorWeapon;
  for(int i=0;i<11;++i){double v=get<double>(warhead+0xA0+i*8);if(v>0&&v<1000000)w.selectMask|=1u<<i;if(v>0.02&&v<1000000)w.armorMask|=1u<<i;}return w;
 }
 assist::planner::Actor combat(uint32_t p,int frame,bool weapons)const{
  using namespace assist::planner;Actor a{};int k=kind(p);uint32_t t=type(p,k);if(k<0||!t)return a;
  a.id=get<uint32_t>(p+0x10);a.type=t;a.kind=k;a.x=get<int>(p+0x9C);a.y=get<int>(p+0xA0);a.z=get<int>(p+0xA4);a.armor=get<int>(t+0x9C,-1);
  if(get<uint8_t>(p+0x1C8))a.flags|=Disabled;if(get<uint32_t>(p+0x11C))a.flags|=Transported;if(get<int>(p+0x220)!=0)a.flags|=Cloaked;
  if(get<uint8_t>(p+0x81))a.flags|=Limbo;if(!get<uint8_t>(p+0x90))a.flags|=OffMap;
  if(get<uint8_t>(t+0xD35))a.flags|=PsionicImmune;if(get<uint8_t>(t+0xD36))a.flags|=PsychicImmune;
  if(!get<uint8_t>(t+0xD3A))a.flags|=TemporalImmune;if(!get<uint8_t>(t+0xD38))a.flags|=ParasiteImmune;
  if(get<uint32_t>(p+0x2C0)&&get<uint8_t>(p+0x2C4))a.flags|=Controlled;if(k!=3&&get<uint32_t>(p+0x694))a.flags|=Infested;
  if(get<uint8_t>(p+0x270)||get<uint32_t>(p+0x278))a.flags|=TemporalTarget;
  if(k==0||get<uint8_t>(p+0x134)||get<int>(p+0x94)==3||get<uint8_t>(p+0x84)||get<uint8_t>(p+0x8D))a.flags|=Airborne;
  if(get<uint32_t>(p+0x2B0)||get<uint8_t>(p+0x427)||get<uint32_t>(p+0x428)||(k!=3&&get<uint8_t>(p+0x6AD)))a.flags|=Manipulated;
  if(get<uint8_t>(t+0xD97))a.flags|=Organic;
  if(assist::remaining(get<int>(p+0x18C),get<int>(p+0x194),frame)||get<uint8_t>(t+0x233)||!get<uint8_t>(t+0x231))a.flags|=Protected;
  if(weapons){int slot[2]={0,1},count=2;
   if(get<uint8_t>(t+0xCD5)){int stage=get<int>(p+0x140,-1);slot[0]=stage>=0&&stage<9?stage*2:-1;slot[1]=slot[0]+1;a.flags|=StagedWeapon;}
   else if(get<int>(t+0x808)>0){slot[0]=get<int>(p+0x138);if(slot[0]==-1)slot[0]=0;count=1;a.flags|=FixedWeapon;}
   if(get<int>(t+0x684,-1)>=0&&get<int>(p+0x2FC)<=0)a.flags|=OutOfAmmo;
   bool elite=get<float>(p+0x150)>=2;for(int i=0;i<count;++i){if(slot[i]<0||slot[i]>=18)continue;uint32_t w=get<uint32_t>(t+0x898+slot[i]*28);uint32_t ew=elite?get<uint32_t>(t+0xA94+slot[i]*28):0;if(ew)w=ew;a.weapons[i]=weapon(w);a.weapons[i].slot=slot[i];}
  }return a;
 }
};
}
