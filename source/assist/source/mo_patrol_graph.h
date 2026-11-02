#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>
namespace assist { namespace mopatrol {
// Read-only native PlanningToken/Node admission. Unknown memory is never an
// empty graph. All consumed bytes are re-read before a decision is returned.
struct Actor {uint32_t pointer=0,id=0;};
struct Path {uint32_t token=0;std::vector<uint32_t> nodes;int pending=-1,length=-1,start=-1;};
template<class Read>class Capture {
 Read read_;struct Block {uint32_t at;std::vector<uint8_t> bytes;};std::vector<Block> blocks_;size_t bytes_=0;
public:
 explicit Capture(Read read):read_(read){}
 bool read(uint32_t at,void*out,size_t size){
  if(at<0x10000||uint64_t(at)+size>0x100000000ull||blocks_.size()>=65536||bytes_+size>4*1024*1024)return false;
  Block b{at,std::vector<uint8_t>(size)};if(!read_(at,b.bytes.data(),size))return false;
  std::memcpy(out,b.bytes.data(),size);bytes_+=size;blocks_.push_back(std::move(b));return true;
 }
 template<class T>bool value(uint32_t at,T&out){return read(at,&out,sizeof out);}
 bool vector(uint32_t at,std::vector<uint32_t>&out,int limit){
  std::array<uint8_t,24>b{};if(!read(at,b.data(),b.size()))return false;uint32_t items=0;int capacity=0,count=0;
  std::memcpy(&items,b.data()+4,4);std::memcpy(&capacity,b.data()+8,4);std::memcpy(&count,b.data()+16,4);
  if(count<0||count>limit||capacity<count||capacity>1048576||(count&&items<0x10000))return false;
  out.resize(size_t(count));return !count||read(items,out.data(),out.size()*4);
 }
 bool path(Actor actor,uint32_t house,Path&out){
  uint32_t id=0,owner=0;out={};if(!value(actor.pointer+0x10,id)||id!=actor.id||!value(actor.pointer+0x21C,owner)||owner!=house||!value(actor.pointer+0x514,out.token))return false;
  if(!out.token)return true;uint32_t back=0;int states[3]{};
  if(!value(out.token,back)||back!=actor.pointer||!vector(out.token+4,out.nodes,1024)||!read(out.token+0x8C,states,sizeof states))return false;
  out.pending=states[0];out.length=states[1];out.start=states[2];
  if(out.pending < -1||out.pending>int(out.nodes.size())||out.length < -1||out.length>int(out.nodes.size())||out.start < -1||out.start>=int(out.nodes.size()))return false;
  for(size_t i=0;i<out.nodes.size();++i)if(out.nodes[i]<0x10000||std::find(out.nodes.begin(),out.nodes.begin()+i,out.nodes[i])!=out.nodes.begin()+i)return false;
  return true;
 }
 bool stable(){std::vector<uint8_t> after;for(const auto&b:blocks_){after.resize(b.bytes.size());if(!read_(b.at,after.data(),after.size())||after!=b.bytes)return false;}return true;}
};
template<class Read>bool noOtherPending(Capture<Read>&c,uint32_t house,const Actor*actors,int count){
 std::vector<uint32_t>tokens;if(!c.vector(0xAC4C78,tokens,8192))return false;
 for(uint32_t token:tokens){uint32_t actor=0,owner=0,back=0;int pending=-1;
  if(!c.value(token,actor)||actor<0x10000||!c.value(actor+0x21C,owner)||!c.value(actor+0x514,back)||back!=token)return false;
  if(owner!=house)continue;bool selected=false;for(int i=0;i<count;++i)selected|=actors[i].pointer==actor;
  if(!selected&&(!c.value(token+0x8C,pending)||pending>=0||pending < -1))return false;
 }return true;
}
template<class Read>bool empty(Read read,uint32_t house,const Actor*actors,int count){
 if(!actors||count<1||count>256)return false;Capture<Read> c(read);
 for(int i=0;i<count;++i){Path p;if(!c.path(actors[i],house,p)||!p.nodes.empty()||p.pending!=-1||p.length!=-1||p.start!=-1)return false;}
 return noOtherPending(c,house,actors,count)&&c.stable();
}
template<class Read>bool route(Read read,uint32_t house,Actor actor,const uint32_t*cells,int count,Path&out,bool closed,const Path*bound=nullptr){
 if(!cells||count<4||count>16)return false;Capture<Read> c(read);Path next;
 if(!c.path(actor,house,next)||!next.token||int(next.nodes.size())!=count||next.pending!=0)return false;
 if(bound&&(next.token!=bound->token||next.nodes!=bound->nodes))return false;
 if(closed){if(next.start!=0||next.length!=count)return false;}
 else if(next.start!=-1||next.length!=-1)return false;
 for(int i=0;i<count;++i){std::vector<uint32_t>members;if(!c.vector(next.nodes[i],members,8192))return false;bool found=false;
  for(auto member:members){uint32_t pair[2]{};if(!c.read(member,pair,sizeof pair))return false;if(pair[0]!=actor.pointer)continue;if(found)return false;found=true;
   std::array<uint8_t,30>event{};if(!c.read(pair[1],event.data(),event.size()))return false;uint32_t id=0,destination=0,nearest=0,target=0;
   std::memcpy(&id,event.data()+7,4);std::memcpy(&target,event.data()+14,4);std::memcpy(&destination,event.data()+19,4);std::memcpy(&nearest,event.data()+24,4);
   // PlanConnect converts the connected suffix to mission 29. A fresh loop
   // beginning at index zero therefore converts every retained Move node.
   if(event[0]!=4||id!=actor.id||event[11]!=0x34||event[12]!=(closed?29:2)||target||event[18]||destination!=cells[i]||event[23]!=11||nearest!=cells[i]||event[28]!=11)return false;
  }if(!found)return false;
 }
 if(!c.stable())return false;out=std::move(next);return true;
}
template<class Read>bool exclusive(Read read,uint32_t house,const Actor*actors,int count){Capture<Read> c(read);return noOtherPending(c,house,actors,count)&&c.stable();}
}} // assist::mopatrol
