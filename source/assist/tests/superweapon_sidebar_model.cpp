#include "../source/superweapon_sidebar_model.h"
#include <cassert>
#include <map>
#include <cstdio>
struct Memory {
 std::map<uint32_t,uint8_t> b;int frameReads=0,arrayReads=0;bool torn=false,tornArray=false;uint32_t failAt=0;
 template<class T>void put(uint32_t p,T v){auto*q=reinterpret_cast<uint8_t*>(&v);for(size_t n=0;n<sizeof v;++n)b[p+uint32_t(n)]=q[n];}
 void bytes(uint32_t p,const void*v,size_t size){auto*q=static_cast<const uint8_t*>(v);for(size_t n=0;n<size;++n)b[p+uint32_t(n)]=q[n];}
 bool read(uint32_t p,void*out,size_t size){if(failAt&&p==failAt)return false;if(p==0xA8ED84&&++frameReads>1&&torn){int frame=103;std::memcpy(out,&frame,4);return true;}auto*q=static_cast<uint8_t*>(out);for(size_t n=0;n<size;++n){auto it=b.find(p+uint32_t(n));if(it==b.end())return false;q[n]=it->second;}if(p==0x30000&&++arrayReads>1&&tornArray)q[1]^=1;return true;}
};
int main(){
 Memory m;constexpr uint32_t house=0x20000,items=0x30000,base=0x50000,types=0x100000;
 m.put<uint32_t>(0xA83D4C,house);m.put<int>(0xA8ED84,100);m.put<uint32_t>(house+0x258,items);m.put<int>(house+0x25C,64);m.put<int>(house+0x264,44);
 m.put<int>(0x8A00A4,2560);m.put<int>(0x8A00A8,1440);m.put<int>(0x886FA0,0);m.put<int>(0xB0CE30,2400);
 for(int n=0;n<44;++n){uint32_t p=base+n*0x100,t=types+n*0x200;m.put<uint32_t>(items+n*4,p);m.put<uint32_t>(p+0x2C,house);m.put<uint8_t>(p+0x6D,1);m.put<uint32_t>(p+0x28,t);char id[24]{};std::snprintf(id,sizeof id,"Super%d",n);m.bytes(t+0x24,id,24);m.put<uint8_t>(t+0xF5,0);m.put<uint32_t>(t+0x60,0);m.put<int>(p+0x30,50);m.put<int>(p+0x38,150);m.put<int>(p+0x24,200);m.put<uint8_t>(p+0x6F,0);m.put<uint8_t>(p+0x70,0);m.put<uint8_t>(t+0xE5,0);m.put<uint32_t>(t+0xB8,0);}
 // Three objects must not appear: a different house, absent, manual-only.
 m.put<uint32_t>(base+40*0x100+0x2C,house+0x1000);m.put<uint8_t>(base+41*0x100+0x6D,0);m.put<uint8_t>(types+42*0x200+0xF5,1);
 // The last object uses a type-level timer and charge-drain readiness.
 m.put<int>(base+43*0x100+0x24,-1);m.put<int>(types+43*0x200+0xB0,200);m.put<uint8_t>(types+43*0x200+0xE5,1);m.put<int>(base+43*0x100+0x7C,2);
 swsidebar::Read read=[&](uint32_t p,void*out,size_t size){return m.read(p,out,size);};std::vector<swsidebar::Entry> entries;int w=0,h=0,right=0;
 assert(swsidebar::sample(read,entries,w,h,right));assert(entries.size()==41);assert(w==2560&&h==1440&&right==2400);assert(entries[0].frames==100&&entries[0].percent==50&&!entries[0].ready);assert(entries.back().index==43&&entries.back().ready&&entries.back().draining);
 m.put<uint8_t>(base+0x70,1);m.put<uint8_t>(base+0x6F,1);m.frameReads=0;assert(swsidebar::sample(read,entries,w,h,right));assert(entries[0].suspended&&!entries[0].ready);
 // Never publish a frame assembled across an owner/frame/vector change.
 m.frameReads=0;m.torn=true;assert(!swsidebar::sample(read,entries,w,h,right));m.torn=false;m.frameReads=0;
 const auto last=entries;
 m.failAt=base+0x38;assert(!swsidebar::sample(read,entries,w,h,right)&&entries==last);m.failAt=0;m.frameReads=0;
 m.tornArray=true;m.arrayReads=0;assert(!swsidebar::sample(read,entries,w,h,right)&&entries==last);m.tornArray=false;m.frameReads=0;
 // A valid display entry is not an input lease: sample live identity/readiness
 // at release, then Runtime independently checks the queued instance/type.
 auto selected=entries.back();assert(swsidebar::selectable(read,selected));
 auto replaced=selected;replaced.instance+=0x100;assert(!swsidebar::sameSelection(selected,replaced)&&!swsidebar::selectable(read,replaced));
 replaced=selected;replaced.type+=0x200;assert(!swsidebar::selectable(read,replaced));
 replaced=selected;replaced.house+=0x100;assert(!swsidebar::selectable(read,replaced));
 m.put<uint8_t>(selected.instance+0x70,1);assert(!swsidebar::selectable(read,selected));m.put<uint8_t>(selected.instance+0x70,0);
 m.failAt=selected.instance+0x28;assert(!swsidebar::selectable(read,selected));m.failAt=0;
 m.put<int>(house+0x25C,43);assert(!swsidebar::selectable(read,selected));m.put<int>(house+0x25C,64);
 auto second=selected;selected.ready=second.ready=false;selected.draining=second.draining=false;selected.frames=16;second.frames=15;assert(!(selected==second));
 // A 50% map-only viewport has logical R wider than the physical screen A.
 m.put<int>(0xB0CE30,4800);assert(swsidebar::sample(read,entries,w,h,right));assert(w==2560&&right==4800);
 m.frameReads=0;m.put<int>(0xB0CE30,8193);assert(!swsidebar::sample(read,entries,w,h,right));m.put<int>(0xB0CE30,2400);
 m.put<int>(house+0x264,257);assert(!swsidebar::sample(read,entries,w,h,right));m.put<int>(house+0x264,44);m.put<int>(house+0x25C,43);assert(!swsidebar::sample(read,entries,w,h,right));
 std::puts("superweapon sidebar model: 41 entries; filtering, charge/drain, suspension, exact countdown change, read-failure retention, vector/owner/frame coherence and release identity/readiness gates passed");
}
