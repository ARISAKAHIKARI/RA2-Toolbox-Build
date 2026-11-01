#pragma once
#include "native_placement_policy.h"
#include "../../external_reader/source/map_bounds.h"
namespace assist { namespace nativeplacement {
template<class Read,class T>bool field(Read&read,uint32_t at,T&value){return at>=0x10000&&uint64_t(at)+sizeof(T)<0x80000000ull&&read(at,&value,sizeof value);}
// Read-only counterpart of Factory::IsDone/GetObject for a completed building.
// 4CA130 requires Production.Value == 54. Its SpecialItem-only alternative is
// deliberately excluded: this operation requires a live Building product.
template<class Read,class Lookup>bool captureOnce(Read&read,Lookup&lookup,uint32_t house,int tab,
 uint32_t match,uint32_t now,int frame,uint64_t hash,Offer&out){
 if(house<0x10000||tab<0||tab>1)return false;
 Offer o{};o.match=match;o.tick=now;o.frame=frame;o.tab=tab;o.house=house;o.typeHash=hash;
 uint32_t owner=0,vt=0;int progress=0,hp=0,buildCat=0;uint8_t alive=0,limbo=0,onMap=1,naval=0;
 if(!field(read,house+0x10,o.houseID)||!field(read,house+(tab?0x53CC:0x53BC),o.factory)||o.factory<0x10000||
    !field(read,o.factory+0x10,o.factoryID)||!field(read,o.factory+0x6C,owner)||owner!=house||
    !field(read,o.factory+0x24,progress)||progress!=54||!field(read,o.factory+0x58,o.product)||o.product<0x10000||
    !field(read,o.product,vt)||vt!=0x7E3EBC||!field(read,o.product+0x10,o.productID)||
    !field(read,o.product+0x21C,owner)||owner!=house||!field(read,o.product+0x6C,hp)||hp<=0||
    !field(read,o.product+0x90,alive)||!alive||!field(read,o.product+0x81,limbo)||!limbo||
    !field(read,o.product+0x74,onMap)||onMap||!field(read,o.product+0x520,o.type)||o.type<0x10000||
    !field(read,o.type+0x10,o.typeID)||!field(read,o.type+0xE08,buildCat)||(buildCat==5?1:0)!=tab||
    !field(read,o.type+0xCCE,naval)||naval>1||!lookup(o.type,o.slot,o.typeIndex,o.naval)||o.naval!=int(naval))return false;
 if(!valid(o))return false;out=o;return true;
}
template<class Read,class Lookup>bool capture(Read read,Lookup lookup,uint32_t house,int tab,
 uint32_t match,uint32_t now,int frame,uint64_t hash,Offer&out){
 Offer a{},b{};
 if(!captureOnce(read,lookup,house,tab,match,now,frame,hash,a)||
    !captureOnce(read,lookup,house,tab,match,now,frame,hash,b)||!sameIdentity(a,b))return false;
 out=b;return true;
}
template<class Read>bool targetUsable(Read read,int x,int y){
 if(!cell(x,y))return false;
 const auto bounds=external::mapbounds::sample(read);if(!bounds.valid())return false;
 uint32_t cells=0,p=0,again=0;int count=0;int16_t xy[2]{};int8_t level=0;uint8_t slope=0;
 const uint32_t index=uint32_t(y*512+x);
 if(!field(read,0x87F924,cells)||cells<0x10000||!field(read,0x87F928,count)||count<=int(index)||count>512*512||
    !field(read,cells+index*4,p)||p<0x10000||!read(p+0x24,xy,sizeof xy)||xy[0]!=x||xy[1]!=y||
    !field(read,p+0x11B,level)||!field(read,p+0x11C,slope)||!bounds.usable(x,y,level,slope!=0)||
    !field(read,cells+index*4,again)||again!=p||!field(read,0x87F924,again)||again!=cells)return false;
 int after=0;return field(read,0x87F928,after)&&after==count&&external::mapbounds::sample(read)==bounds;
}
}} // assist::nativeplacement
