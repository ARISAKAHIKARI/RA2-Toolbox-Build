// Own Windows x86 process; real DR/VEH and exact production transaction core.
#define main legacy_fixture_main_not_called
#include "tech_ui_breakpoint_windows.cpp"
#undef main
#include "../source/tech_enhancement_parameter_policy.h"
struct ExactType {int tag,level;};static ExactType exactAllowed{21,-1},exactDenied{22,-1};
extern "C" __declspec(naked) int __thiscall ExactCanBuild(House*,ExactType*,int){__asm {
 push ebx
 mov ebx,ecx
 mov eax,[esp+8]
 cmp dword ptr [eax+4],-1
 jne levelPresent
 xor eax,eax
 pop ebx
 ret 8
levelPresent:
 mov ecx,[esp+0Ch]
 cmp dword ptr [eax+4],ecx
 jg levelDenied
 mov eax,1
 pop ebx
 ret 8
levelDenied:
 xor eax,eax
 pop ebx
 ret 8
}}
static bool joint(void*raw,uint32_t tag){
 if(tag<2)return apply(raw,tag);auto&c=*static_cast<CONTEXT*>(raw);
 if(c.Ebx!=uint32_t(uintptr_t(&local))||c.Eax!=uint32_t(uintptr_t(&exactAllowed)))return false;
 const DWORD before=c.EFlags;c.EFlags=assist::techenhanceparams::compareFlags(1,tag==2?UINT32_MAX:c.Ecx,c.EFlags);
 assert((before&~0x8D5u)==(c.EFlags&~0x8D5u));return true;
}
static unsigned findCode(const uint8_t*bytes,unsigned count){const auto*p=reinterpret_cast<const uint8_t*>(&ExactCanBuild);unsigned found=0;for(unsigned i=0;i<80;++i)if(!memcmp(p+i,bytes,count)){assert(!found);found=uint32_t(uintptr_t(p+i+count));}assert(found);return found;}
static void configured(assist::techbp::Service&s,const uint32_t*sites,unsigned wanted){printf("configure requested=%u prior=%u\n",wanted,s.activeTags());assert(s.configure(sites,wanted,joint));wait(s);printf("configure done state=%ld active=%u mask=%u\n",s.state,s.activeTags(),s.lease.mask);}
static double calls(unsigned count,bool active){LARGE_INTEGER f{},a{},b{};assert(QueryPerformanceFrequency(&f)&&QueryPerformanceCounter(&a));for(unsigned n=0;n<count;++n){assert(DrawCaller(&local,&allowed)==(active?1:0));assert(PruneCaller(&local,&allowed)==(active?3:2));assert(ExactCanBuild(&local,&exactAllowed,1)==(active?1:0));assert(ExactCanBuild(&local,&exactAllowed,0)==0);}assert(QueryPerformanceCounter(&b));return double(b.QuadPart-a.QuadPart)*1e6/double(f.QuadPart);}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);setbuf(stdout,nullptr);CONTEXT original=debug();assert(!original.Dr0&&!original.Dr1&&!original.Dr2&&!original.Dr3);
 assert(DrawCaller(&local,&allowed)==0);uint32_t a=returned-5;assert(PruneCaller(&local,&allowed)==2);uint32_t b=returned-5;
 const uint8_t first[]={0x83,0x78,0x04,0xFF},second[]={0x39,0x48,0x04};const uint32_t sites[4]={a,b,findCode(first,sizeof first),findCode(second,sizeof second)};
 assert((*reinterpret_cast<uint8_t*>(uintptr_t(sites[2]))&0xF0)==0x70&&(*reinterpret_cast<uint8_t*>(uintptr_t(sites[3]))&0xF0)==0x70);
 uint8_t codeBefore[80];memcpy(codeBefore,reinterpret_cast<void*>(&ExactCanBuild),sizeof codeBefore);ExactType typeBefore=exactAllowed;
 assist::techbp::Service service;const double baseline=calls(10000,false);
 // One slot only virtualizes the -1 gate; the native second comparison still
 // sees -1. The optional second slot compares level 1 and rejects House 0.
 configured(service,sites,4);assert(service.activeTags()==4&&__builtin_popcount(service.lease.mask)==1);assert(ExactCanBuild(&local,&exactAllowed,0)==1&&ExactCanBuild(&local,&exactAllowed,1)==1);
 exactAllowed.level=11;assert(ExactCanBuild(&local,&exactAllowed,5)==0);configured(service,sites,12);assert(ExactCanBuild(&local,&exactAllowed,5)==1&&ExactCanBuild(&local,&exactAllowed,0)==0);exactAllowed=typeBefore;retire(service);
 CONTEXT foreign=original;foreign.Dr0=DWORD(uintptr_t(&foreignMarker));foreign.Dr1=foreign.Dr0+1;foreign.Dr7|=1|4;debug(&foreign);
 configured(service,sites,3);assert(service.activeTags()==3);CONTEXT fullOnly=debug();assert(DrawCaller(&local,&allowed)==1);
 configured(service,sites,15);assert(service.state==assist::techbp::Slots&&service.activeTags()==3);CONTEXT refused=debug();assert(refused.Dr0==fullOnly.Dr0&&refused.Dr1==fullOnly.Dr1&&refused.Dr2==fullOnly.Dr2&&refused.Dr3==fullOnly.Dr3&&refused.Dr7==fullOnly.Dr7);assert(DrawCaller(&local,&allowed)==1&&ExactCanBuild(&local,&exactAllowed,1)==0);
 // Relinquish only the fixture's own foreign slots, retain active full group.
 CONTEXT available=refused;available.Dr0=available.Dr1=0;available.Dr7&=~(assist::techbp::slotMask(0)|assist::techbp::slotMask(1));debug(&available);
 configured(service,sites,15);assert(service.activeTags()==15&&service.state==assist::techbp::Ready);CONTEXT both=debug();assert(both.Dr2==fullOnly.Dr2&&both.Dr3==fullOnly.Dr3);
 assert(ExactCanBuild(&local,&exactAllowed,1)==1&&ExactCanBuild(&local,&exactAllowed,0)==0&&ExactCanBuild(&other,&exactAllowed,1)==0&&ExactCanBuild(&local,&exactDenied,1)==0);
 assert(!memcmp(&typeBefore,&exactAllowed,sizeof typeBefore)&&!memcmp(codeBefore,reinterpret_cast<void*>(&ExactCanBuild),sizeof codeBefore));
 LONG hits=service.exceptions,changes=service.changed;const double active=calls(10000,true);assert(service.exceptions-hits==60000&&service.changed-changes==60000);
 printf("JOINT_QPC iterations=10000 exceptions=60000 baseline_us=%.3f active_us=%.3f additional_us_per_exception=%.6f\n",baseline,active,(active-baseline)/60000.0);
 configured(service,sites,3);assert(service.activeTags()==3&&ExactCanBuild(&local,&exactAllowed,1)==0&&DrawCaller(&local,&allowed)==1);CONTEXT noEnhance=debug();assert(noEnhance.Dr2==fullOnly.Dr2&&noEnhance.Dr3==fullOnly.Dr3);
 configured(service,sites,12);assert(service.activeTags()==12&&ExactCanBuild(&local,&exactAllowed,1)==1&&DrawCaller(&local,&allowed)==0);
 // Ambiguous same-slot reconfiguration: preserve old+new possible ownership
 // until an actual readback proves which addresses the kernel installed.
 contextReads=0;failReadAt=2;configured(service,sites,3);assert(service.state==assist::techbp::System&&service.lease.uncertain&&service.activeTags()==0&&service.resources());failReadAt=0;
 assert(DrawCaller(&local,&allowed)==0);retire(service);CONTEXT empty=debug();assert(!empty.Dr0&&!empty.Dr1&&!empty.Dr2&&!empty.Dr3);
 configured(service,sites,12);skipSetOnce=2;configured(service,sites,3);assert(service.state==assist::techbp::System&&service.activeTags()==12&&service.resources());assert(ExactCanBuild(&local,&exactAllowed,1)==0&&DrawCaller(&local,&allowed)==0);retire(service);
 assert(!memcmp(&typeBefore,&exactAllowed,sizeof typeBefore)&&!memcmp(codeBefore,reinterpret_cast<void*>(&ExactCanBuild),sizeof codeBefore));CONTEXT after=debug();assert(after.Dr0==original.Dr0&&after.Dr1==original.Dr1&&after.Dr2==original.Dr2&&after.Dr3==original.Dr3&&after.Dr7==original.Dr7);
 puts("PASS: four-site real Windows DR transaction, retained existing full pair on insufficient slots, exact flags/House/Type/TechLevel gate, 60000 quantified exceptions, partial removal, ambiguous readback and false Set restore, unchanged code/type, no game");return 0;
}
