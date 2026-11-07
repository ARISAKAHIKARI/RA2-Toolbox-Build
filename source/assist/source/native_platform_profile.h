#pragma once
#include "native_entry_policy.h"
#include <string.h>

// Ordinary-call evidence only. No writer, DR consumer, or generic jump resolver
// may use this as permission to replace instructions. The caller keeps the
// original function entry so the platform callback and continuation both run.
namespace nativeplatform {
constexpr uint32_t UnselectAll=0x48DC90,Select=0x6FBFA0;
constexpr int slot(uint32_t entry){return entry==nativeentry::CanBuild?0:entry==UnselectAll?1:entry==Select?2:-1;}
constexpr bool known(uint32_t entry){return slot(entry)>=0;}
enum class Failure:uint32_t {
 None,NotProfile,EntryUnreadable,EntryLayout,SiteJump,StubUnreadable,StubShape,
 UnknownCallbackChain,Replay,Continuation,ModuleIdentity,ModuleLayout,
 HookDeclaration,Export,Capabilities,CallbackOwner,CallbackShape,Companion,Changed
};
constexpr int ErrorBase=9300;
inline const char*failureName(Failure value){
 switch(value){
 case Failure::None:return "matched";case Failure::NotProfile:return "not-profile";
 case Failure::EntryUnreadable:return "entry-unreadable";case Failure::EntryLayout:return "entry-layout";
 case Failure::SiteJump:return "site-jump";case Failure::StubUnreadable:return "stub-unreadable";
 case Failure::StubShape:return "wrapper-shape-or-ABI";case Failure::UnknownCallbackChain:return "unknown-callback-chain";
 case Failure::Replay:return "relocated-original-bytes";case Failure::Continuation:return "continuation";
 case Failure::ModuleIdentity:return "callback-not-loaded-image";case Failure::ModuleLayout:return "module-layout";
 case Failure::HookDeclaration:return "hook-declaration";case Failure::Export:return "named-export";
 case Failure::Capabilities:return "loader-capabilities";case Failure::CallbackOwner:return "callback-owner";
 case Failure::CallbackShape:return "callback-entry-shape";case Failure::Companion:return "CanBuild-companion";
 case Failure::Changed:return "changed-during-inspection";
 }return "unknown-profile-error";
}
struct Inspection {
 uint32_t entry=0,site=0,stub=0,callback=0,module=0,moduleBytes=0,timestamp=0;
 Failure failure=Failure::None,detail=Failure::None;uint32_t observedBytes=0;
 uint8_t observed[16]{};
};
struct Profile {
 uint32_t entry,site;uint8_t prefix,hookBytes,bytes;const char*name;uint8_t original[12];
};
// Phobos ce864b03 / SyringeEx 7dbb780: these sites overwrite only complete
// non-relative instructions. Other loader encodings and added callbacks fail
// closed; declaration/export names are a contract, not a version/hash claim.
constexpr Profile Profiles[]={
 {nativeentry::CanBuild,0x4F7877,7,5,12,"HouseClass_CanBuild_UpgradesInteraction_WithoutAres",
  {0x83,0xEC,0x3C,0x8A,0x44,0x24,0x44,0x53,0x55,0x8B,0xE9,0x56}},
 {UnselectAll,UnselectAll,0,5,8,"MapClass_UnselectAll_ClearLimboLaunchers",
  {0xA1,0xC8,0xEC,0xA8,0x00,0x85,0xC0,0x74}},
 {Select,0x6FBFA3,3,6,9,"TechnoClass_Select_SkipLimboDelivery",
  {0x56,0x8B,0xF1,0x8A,0x86,0x1B,0x04,0x00,0x00}}
};
constexpr uint32_t CompanionSite=0x4F8361;
constexpr const char*CompanionName="HouseClass_CanBuild_UpgradesInteraction";
constexpr uint8_t CompanionOriginal[]={0xC2,0x0C,0x00,0x6E,0x7D};
inline uint16_t half(const uint8_t*p){return uint16_t(p[0])|uint16_t(p[1])<<8;}
inline bool reject(Inspection&out,Failure why){out.failure=why;return false;}
inline bool target(uint32_t instruction,const uint8_t*bytes,uint32_t&out){
 const int64_t next=int64_t(instruction)+5+int32_t(nativeentry::word(bytes+1));
 if(bytes[0]!=0xE9&&bytes[0]!=0xE8)return false;
 if(next<0x10000||next>=0x80000000ll)return false;out=uint32_t(next);return true;
}
struct Image {
 uint32_t base=0,bytes=0,timestamp=0,hooks=0,hookBytes=0,exports=0,exportBytes=0;
 bool contains(uint32_t at,size_t count)const{
  return count&&at>=base&&uint64_t(at)+count<=uint64_t(base)+bytes&&nativeentry::range(at,count);
 }
};
template<class Read,class Owned>bool imageRead(const Image&image,uint32_t at,void*out,size_t bytes,Read read,Owned owned){
 return image.contains(at,bytes)&&owned(image.base,at,bytes,false)&&read(at,out,bytes);
}
template<class Read,class Owned>bool image(uint32_t base,Image&out,Read read,Owned owned){
 uint8_t dos[64]{},nt[128]{};
 if(!nativeentry::range(base,sizeof dos)||!owned(base,base,sizeof dos,false)||!read(base,dos,sizeof dos)||half(dos)!=0x5A4D)return false;
 const uint32_t pe=nativeentry::word(dos+60);
 if(pe<64||pe>4096||!nativeentry::range(base+pe,sizeof nt)||!owned(base,base+pe,sizeof nt,false)||!read(base+pe,nt,sizeof nt))return false;
 const uint32_t sections=half(nt+6),optional=half(nt+20),bytes=nativeentry::word(nt+24+56);
 if(nativeentry::word(nt)!=0x4550||half(nt+4)!=0x14C||!(half(nt+22)&0x2000)||half(nt+24)!=0x10B||
    !sections||sections>96||optional<224||optional>4096||bytes<4096||bytes>0x10000000||!nativeentry::range(base,bytes)||nativeentry::word(nt+24+92)<1)return false;
 out.base=base;out.bytes=bytes;out.timestamp=nativeentry::word(nt+8);
 const uint32_t exportRva=nativeentry::word(nt+24+96),exportBytes=nativeentry::word(nt+24+100);
 if(exportRva>=bytes||exportBytes<40||uint64_t(exportRva)+exportBytes>bytes)return false;
 out.exports=base+exportRva;out.exportBytes=exportBytes;
 const uint32_t sectionTable=base+pe+24+optional;
 if(!out.contains(sectionTable,size_t(sections)*40))return false;
 for(uint32_t i=0;i<sections;++i){
  uint8_t section[40]{};if(!imageRead(out,sectionTable+i*40,section,sizeof section,read,owned))return false;
  if(memcmp(section,".syhks00",8))continue;
  const uint32_t size=nativeentry::word(section+8),rva=nativeentry::word(section+12);
  if(out.hooks||!size||size%16||size>0x20000||rva>=bytes||uint64_t(rva)+size>bytes||!(nativeentry::word(section+36)&0x40000000))return false;
  out.hooks=base+rva;out.hookBytes=size;
 }
 return out.hooks!=0;
}
// Compare only the bounded prefix needed by a particular export name. Export
// name RVAs and the in-memory relocated hook-name pointer must stay in image.
template<class Read,class Owned>bool compareName(const Image&module,uint32_t at,const char*expected,int&comparison,Read read,Owned owned){
 const size_t bytes=strlen(expected)+1;uint8_t name[96]{};
 if(bytes>sizeof name||!imageRead(module,at,name,bytes,read,owned))return false;
 for(size_t i=0;i<bytes;++i){comparison=int(name[i])-int(uint8_t(expected[i]));if(comparison||!name[i])return true;}
 comparison=0;return true;
}
template<class Read,class Owned>bool namedExport(const Image&module,const char*name,uint32_t&address,Read read,Owned owned){
 uint8_t directory[40]{};if(!imageRead(module,module.exports,directory,sizeof directory,read,owned))return false;
 const uint32_t functions=nativeentry::word(directory+20),names=nativeentry::word(directory+24);
 const uint32_t fnRva=nativeentry::word(directory+28),nameRva=nativeentry::word(directory+32),ordRva=nativeentry::word(directory+36);
 if(!functions||functions>32768||!names||names>32768||fnRva>=module.bytes||nameRva>=module.bytes||ordRva>=module.bytes||
    !module.contains(module.base+fnRva,size_t(functions)*4)||!module.contains(module.base+nameRva,size_t(names)*4)||!module.contains(module.base+ordRva,size_t(names)*2))return false;
 uint32_t first=0,last=names;
 while(first<last){
  const uint32_t middle=first+(last-first)/2;uint8_t raw[4]{};
  if(!imageRead(module,module.base+nameRva+middle*4,raw,4,read,owned))return false;
  const uint32_t rva=nativeentry::word(raw);int comparison=0;
  if(rva>=module.bytes||!compareName(module,module.base+rva,name,comparison,read,owned))return false;
  if(comparison<0){first=middle+1;continue;}if(comparison>0){last=middle;continue;}
  if(!imageRead(module,module.base+ordRva+middle*2,raw,2,read,owned))return false;
  const uint32_t ordinal=half(raw);if(ordinal>=functions||!imageRead(module,module.base+fnRva+ordinal*4,raw,4,read,owned))return false;
  const uint32_t functionRva=nativeentry::word(raw);if(!functionRva||functionRva>=module.bytes)return false;
  address=module.base+functionRva;
  // A forwarded export string is not a code or capability address.
  return !(address>=module.exports&&uint64_t(address)<uint64_t(module.exports)+module.exportBytes);
 }
 return false;
}
template<class Read,class Owned>bool declaration(const Image&module,uint32_t site,uint32_t bytes,const char*name,Read read,Owned owned){
 unsigned matches=0;uint8_t block[1024]{};
 for(uint32_t offset=0;offset<module.hookBytes;){
  const uint32_t count=module.hookBytes-offset<sizeof block?module.hookBytes-offset:uint32_t(sizeof block);
  if(!imageRead(module,module.hooks+offset,block,count,read,owned))return false;
  for(uint32_t at=0;at<count;at+=16){
   if(nativeentry::word(block+at)!=site)continue;
   int comparison=0;
   if(++matches!=1||nativeentry::word(block+at+4)!=bytes||
      !compareName(module,nativeentry::word(block+at+8),name,comparison,read,owned)||comparison)return false;
  }offset+=count;
 }
 return matches==1;
}
// SyringeEx 7dbb780's complete REGISTERS/cdecl wrapper. Only the origin and
// relative CALL fields vary. In particular, ADD ESP,8, flags, ESP restoration
// and the nonzero handler-return branch are all part of the accepted ABI.
constexpr uint8_t Wrapper[]={
 0x60,0x9C,0x68,0,0,0,0,0x54,0xE8,0,0,0,0,0x83,0xC4,0x08,
 0x64,0xA3,0x14,0,0,0,0x64,0x83,0x3D,0x14,0,0,0,0,0x74,0x18,
 0x9D,0x5F,0x5E,0x5D,0x5B,0x8B,0x44,0x24,0x0C,0x89,0x5C,0x24,0x0C,0x5B,0x5A,0x59,0x5C,
 0x64,0xFF,0x25,0x14,0,0,0,
 0x9D,0x5F,0x5E,0x5D,0x5B,0x8B,0x44,0x24,0x0C,0x89,0x5C,0x24,0x0C,0x5B,0x5A,0x59,0x5C
};
static_assert(sizeof Wrapper==73,"reviewed SyringeEx wrapper length");
struct HookImage {uint32_t site=0,stub=0,callback=0,siteBytes=0,stubBytes=0;uint8_t before[8]{},body[96]{};};
template<class Read,class Executable>bool hookImage(uint32_t site,uint32_t bytes,const uint8_t*original,HookImage&out,Inspection&diagnostic,Read read,Executable executable){
 out.site=site;out.siteBytes=bytes<5?5:bytes;out.stubBytes=uint32_t(sizeof Wrapper)+bytes+5;
 if(!bytes||out.siteBytes>sizeof out.before||out.stubBytes>sizeof out.body||!nativeentry::range(site,out.siteBytes)||
    !executable(site,out.siteBytes)||!read(site,out.before,out.siteBytes))return reject(diagnostic,Failure::EntryUnreadable);
 if(!target(site,out.before,out.stub)||out.before[0]!=0xE9)return reject(diagnostic,Failure::SiteJump);
 for(uint32_t i=5;i<out.siteBytes;++i)if(out.before[i]!=0x90)return reject(diagnostic,Failure::SiteJump);
 diagnostic.stub=out.stub;
 if(!nativeentry::range(out.stub,out.stubBytes)||!executable(out.stub,out.stubBytes)||!read(out.stub,out.body,out.stubBytes))return reject(diagnostic,Failure::StubUnreadable);
 for(size_t i=0;i<sizeof Wrapper;++i){
  if((i>=3&&i<7)||(i>=9&&i<13))continue;
  if(out.body[i]!=Wrapper[i])return reject(diagnostic,Failure::StubShape);
 }
 if(nativeentry::word(out.body+3)!=site||!target(out.stub+8,out.body+8,out.callback))return reject(diagnostic,Failure::StubShape);
 diagnostic.callback=out.callback;
 if(out.body[sizeof Wrapper]==Wrapper[0]&&out.body[sizeof Wrapper+1]==Wrapper[1])return reject(diagnostic,Failure::UnknownCallbackChain);
 if(memcmp(out.body+sizeof Wrapper,original,bytes))return reject(diagnostic,Failure::Replay);
 uint32_t continuation=0;
 if(out.body[sizeof Wrapper+bytes]!=0xE9||!target(out.stub+uint32_t(sizeof Wrapper)+bytes,out.body+sizeof Wrapper+bytes,continuation)||continuation!=site+5)return reject(diagnostic,Failure::Continuation);
 return true;
}
inline bool entryTransfer(const uint8_t*body,size_t bytes){
 size_t at=0;
 while(at<4&&at<bytes&&(body[at]==0x2E||body[at]==0x36||body[at]==0x3E||body[at]==0x26||body[at]==0x64||body[at]==0x65||body[at]==0x66||body[at]==0x67||body[at]==0xF2||body[at]==0xF3))++at;
 if(at>=bytes)return true;
 const auto*b=body+at;const size_t left=bytes-at;
 if(b[0]==0xE9||b[0]==0xEB||b[0]==0xEA)return true;
 if(left>=2&&b[0]==0xFF&&(((b[1]>>3)&7)==4||((b[1]>>3)&7)==5))return true;
 if(left>=6&&b[0]==0x68&&(b[5]==0xC3||b[5]==0xC2))return true;
 if(left>=3&&b[0]==0x6A&&(b[2]==0xC3||b[2]==0xC2))return true;
 if(left>=7&&b[0]>=0xB8&&b[0]<=0xBF){const uint8_t reg=b[0]-0xB8;
  if((b[5]==0xFF&&b[6]==uint8_t(0xE0+reg))||(b[5]==uint8_t(0x50+reg)&&b[6]==0xC3))return true;
 }
 if(left>=7&&b[0]==0xA1&&b[5]==0xFF&&b[6]==0xE0)return true;
 return false;
}
template<class Read,class Owned>bool contract(const Image&module,uint32_t site,uint32_t bytes,const char*name,uint32_t callback,Inspection&out,Read read,Owned owned){
 if(!declaration(module,site,bytes,name,read,owned))return reject(out,Failure::HookDeclaration);
 uint32_t declared=0;if(!namedExport(module,name,declared,read,owned))return reject(out,Failure::Export);
 if(declared!=callback||!module.contains(callback,16)||!owned(module.base,callback,16,true))return reject(out,Failure::CallbackOwner);
 uint8_t body[16]{};
 if(!read(callback,body,sizeof body))return reject(out,Failure::CallbackOwner);
 // Refuse recognized entry-transfer replacements. This finite structural
 // contract does not claim to authenticate every byte/semantic of a DLL body.
 if(!body[0]||body[0]==0xCC||body[0]==0xF4||body[0]==0xC2||body[0]==0xC3||entryTransfer(body,sizeof body))return reject(out,Failure::CallbackShape);
 return true;
}
template<class Read,class Owned>bool capabilities(const Image&module,Read read,Owned owned){
 const char*names[]={"ESPModification","ZFPreservation","ReladdrInstructionFixup"};
 for(const char*name:names){uint32_t at=0;uint8_t enabled=0;
  if(!namedExport(module,name,at,read,owned)||!imageRead(module,at,&enabled,1,read,owned)||enabled!=1)return false;
 }return true;
}
template<class Read,class Executable>bool unchanged(const HookImage&hook,Read read,Executable executable){
 uint8_t site[8]{},body[96]{};
 return executable(hook.site,hook.siteBytes)&&executable(hook.stub,hook.stubBytes)&&
  read(hook.site,site,hook.siteBytes)&&!memcmp(site,hook.before,hook.siteBytes)&&
  read(hook.stub,body,hook.stubBytes)&&!memcmp(body,hook.body,hook.stubBytes);
}
template<class Read,class Executable,class ImageAt,class Owned>bool inspect(uint32_t entry,Inspection&out,Read read,Executable executable,ImageAt imageAt,Owned owned){
 out={};out.entry=entry;const int index=slot(entry);if(index<0)return reject(out,Failure::NotProfile);
 const auto&p=Profiles[index];out.site=p.site;
 if(!executable(entry,p.bytes)||!read(entry,out.observed,p.bytes))return reject(out,Failure::EntryUnreadable);
 out.observedBytes=p.bytes;
 for(uint32_t i=0;i<p.bytes;++i)if((i<p.prefix||i>=uint32_t(p.prefix)+p.hookBytes)&&out.observed[i]!=p.original[i])return reject(out,Failure::EntryLayout);
 HookImage hook;if(!hookImage(p.site,p.hookBytes,p.original+p.prefix,hook,out,read,executable))return false;
 out.module=imageAt(hook.callback);if(!out.module)return reject(out,Failure::ModuleIdentity);
 Image module;if(!image(out.module,module,read,owned))return reject(out,Failure::ModuleLayout);
 out.moduleBytes=module.bytes;out.timestamp=module.timestamp;
 if(!contract(module,p.site,p.hookBytes,p.name,hook.callback,out,read,owned))return false;
 if(!capabilities(module,read,owned))return reject(out,Failure::Capabilities);
 uint8_t companionBefore[sizeof CompanionOriginal]{};HookImage companion;bool wrappedCompanion=false;
 if(entry==nativeentry::CanBuild){
  // This callback restores both itself and 4F8361. Never admit it while its
  // companion is owned by an unknown replacement which that restore would erase.
  if(!executable(CompanionSite,sizeof companionBefore)||!read(CompanionSite,companionBefore,sizeof companionBefore))return reject(out,Failure::Companion);
  if(memcmp(companionBefore,CompanionOriginal,sizeof companionBefore)){
   Inspection detail;
   if(!hookImage(CompanionSite,3,CompanionOriginal,companion,detail,read,executable)||
      imageAt(companion.callback)!=module.base||!contract(module,CompanionSite,3,CompanionName,companion.callback,detail,read,owned)){
    out.detail=detail.failure;return reject(out,Failure::Companion);
   }
   wrappedCompanion=true;
  }
 }
 uint8_t current[16]{};
 if(!unchanged(hook,read,executable)||!read(entry,current,p.bytes)||memcmp(current,out.observed,p.bytes))return reject(out,Failure::Changed);
 if(entry==nativeentry::CanBuild&&(!read(CompanionSite,current,sizeof companionBefore)||memcmp(current,companionBefore,sizeof companionBefore)))return reject(out,Failure::Changed);
 if(wrappedCompanion&&!unchanged(companion,read,executable))return reject(out,Failure::Changed);
 return true;
}
}
