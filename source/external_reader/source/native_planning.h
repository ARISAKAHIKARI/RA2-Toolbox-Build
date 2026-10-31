#pragma once
#include "ground_geometry.h"
#include <map>
#include <set>
#include <tuple>
namespace external {namespace planning {
// GPL-3.0, Vera20K 3d42426: sim/world/planning.rs and
// presentation/render/planning_lines.rs. Read the retained native graph;
// Foot+588 is a navigation queue, not the Z planning graph.
using Point=nativefeedback::Point3;
struct Segment {Point from{},to{};uint32_t owner=0;bool bright=false,loop=false;};
struct Node {uint32_t pointer=0,owner=0;Point point{};std::vector<uint32_t>members;std::vector<Segment>incoming;int badge=0;bool bright=false;};
struct Actor {uint32_t pointer=0,owner=0;bool hasCurrent=false,hasMega=false,hasConnection=false,markerOnly=false;Segment current{},mega{},connection{};Point marker{};int currentBadge=0,badge=0;};
struct Graph {std::vector<Node>nodes;std::vector<Actor>actors;std::map<uint32_t,std::array<uint8_t,3>>colors;std::set<uint32_t>white;uint32_t hover=0,highlight=0;bool active=false;};

// A frame owns its complete read set. Re-read through the uncached reader
// before publishing: a recycled node/event or a changed vector must not draw.
template<class Read>class Snapshot {
 Read read_;std::map<std::pair<uint32_t,size_t>,std::vector<uint8_t>>blocks_;size_t bytes_=0;bool good_=true;
public:
 explicit Snapshot(Read read):read_(read){}
 bool read(uint32_t at,void*out,size_t size){
  if(!size)return true;if(!good_||!nativefeedback::range(at,size))return good_=false;
  auto key=std::make_pair(at,size);auto it=blocks_.find(key);
  if(it==blocks_.end()){if(blocks_.size()>=131072||bytes_+size>8*1024*1024)return good_=false;std::vector<uint8_t>b(size);if(!read_(at,b.data(),size))return good_=false;bytes_+=size;it=blocks_.emplace(key,std::move(b)).first;}
  std::memcpy(out,it->second.data(),size);return true;
 }
 bool stable(){if(!good_)return false;std::vector<uint8_t>after;for(const auto&b:blocks_){after.resize(b.first.second);if(!read_(b.first.first,after.data(),after.size())||after!=b.second)return false;}return true;}
};
struct Path {uint32_t actor=0,owner=0;std::vector<uint32_t>nodes;int pending=-1,loop=-1;bool loopChanged=false,selected=false;};
struct Order {uint32_t actor=0;Point point{};int badge=0;bool resolved=false;};
struct RawNode {uint32_t pointer=0;std::vector<Order>orders;};

template<class Read>bool capture(Read fresh,uint32_t house,Graph&out){
 out={};Snapshot<Read>snapshot(fresh);auto read=[&](uint32_t at,void*p,size_t n){return snapshot.read(at,p,n);};GameView game(read);
 uint8_t active=0;if(!read(0xAC4CF4,&active,1)||active>1)return false;if(!active)return snapshot.stable();
 Graph graph;graph.active=true;
 if(!read(0xAC4CCC,&graph.hover,4)||!read(0xAC4BF0,&graph.highlight,4))return false;
 std::vector<uint32_t>nodePointers,tokens;
 if(!game.vecChecked(0xAC4B30,nodePointers,8192)||!game.vecChecked(0xAC4C78,tokens,8192))return false;
 std::set<uint32_t>uniqueNodes(nodePointers.begin(),nodePointers.end()),uniqueTokens(tokens.begin(),tokens.end());
 if(uniqueNodes.size()!=nodePointers.size()||uniqueTokens.size()!=tokens.size())return false;
 std::map<uint32_t,Path>paths;std::vector<uint32_t>actorOrder;size_t occurrences=0;
 for(uint32_t token:tokens){
  if(!nativefeedback::range(token,0x9C))return false;Path path;
  if(!read(token,&path.actor,4)||!nativefeedback::range(path.actor,0x518)||game.kind(path.actor)<0||game.template get<uint32_t>(path.actor+0x514)!=token)return false;
  path.owner=game.template get<uint32_t>(path.actor+0x21C);if(!game.allied(path.owner,house))continue;
  if(!game.vecChecked(token+4,path.nodes,8192))return false;if(path.nodes.empty())continue;
  occurrences+=path.nodes.size();if(occurrences>65536)return false;
  int state[3]{};uint8_t changed=0;if(!read(token+0x8C,state,sizeof state)||!read(token+0x98,&changed,1)||changed>1)return false;
  path.pending=state[0];path.loop=state[2];path.loopChanged=changed;
  if(path.pending < -1||path.pending>int(path.nodes.size())||path.loop < -1||path.loop>=int(path.nodes.size()))return false;
  for(auto node:path.nodes)if(!uniqueNodes.count(node))return false;
  path.selected=game.template get<uint8_t>(path.actor+0x83)!=0;
  if(!graph.colors.count(path.owner)){std::array<uint8_t,3>rgb{};if(!nativefeedback::range(path.owner,0x56FC)||!read(path.owner+0x56F9,rgb.data(),rgb.size()))return false;graph.colors[path.owner]=rgb;}
  actorOrder.push_back(path.actor);if(!paths.emplace(path.actor,std::move(path)).second)return false;
 }
 // Resolve object NetIDs from the complete live registry, independent of
 // view culling. A distant unit can own a path that crosses this viewport.
 std::map<uint32_t,uint32_t>byID;bool indexed=false;
 auto pointerPoint=[&](uint32_t pointer,Point&point,bool bridge)->bool{
  int kind=game.kind(pointer);
  if(kind>=0){if(!read(pointer+0x9C,&point,sizeof point))return false;if(kind==3){int w=0,h=0;if(!animations::foundation(read,game.type(pointer,kind),w,h))return false;point.x+=(w-1)*128;point.y+=(h-1)*128;}return nativefeedback::worldPoint(point);}
  if(game.template get<uint32_t>(pointer)!=0x7E4EEC)return false;int16_t xy[2]{};if(!read(pointer+0x24,xy,4)||!mapbounds::sample(read).legal(xy[0],xy[1]))return false;point={xy[0]*256+128,xy[1]*256+128,0};uint32_t cell=0;return ground::at(read,point,point.z,cell,bridge)&&cell==pointer;
 };
 auto target=[&](const uint8_t*ref,Point&point)->bool{
  uint32_t id=nativefeedback::u32(ref);if(ref[4]==0x0B){int x=id%1000,y=id/1000;if(x<=0||x>=512||y<=0||y>=512||!mapbounds::sample(read).legal(x,y))return false;point={x*256+128,y*256+128,0};uint32_t cell=0;return ground::at(read,point,point.z,cell);}
  if(ref[4]!=0x34||!id)return false;
  if(!indexed){std::vector<uint32_t>objects;if(!game.vecChecked(0xA8EC78,objects,32768))return false;for(auto object:objects){uint32_t objectID=game.template get<uint32_t>(object+0x10);if(objectID&&game.kind(object)>=0)byID.emplace(objectID,object);}indexed=true;}
  auto it=byID.find(id);return it!=byID.end()&&pointerPoint(it->second,point,true);
 };
 std::map<uint32_t,RawNode>raw;size_t memberships=0;
 for(uint32_t pointer:nodePointers){
  RawNode node;node.pointer=pointer;std::vector<uint32_t>members;
  if(!game.vecChecked(pointer,members,8192))return false;memberships+=members.size();if(memberships>65536)return false;
  for(uint32_t member:members){uint32_t binding[2]{};if(!nativefeedback::range(member,8)||!read(member,binding,sizeof binding))return false;
   Order order;order.actor=binding[0];auto path=paths.find(order.actor);if(path==paths.end())continue;
   if(std::find(path->second.nodes.begin(),path->second.nodes.end(),pointer)==path->second.nodes.end())return false;
   std::array<uint8_t,30>event{};if(!nativefeedback::range(binding[1],event.size())||!read(binding[1],event.data(),event.size())||event[0]!=4||event[11]!=0x34||nativefeedback::u32(event.data()+7)!=game.template get<uint32_t>(order.actor+0x10))return false;
   order.badge=event[12]==11?433:event[12]==29?434:0;
   // Event63B050: TarCom, NavComAux, NavCom. Choose by tag first:
   // a stale non-null target must not silently fall through to a new endpoint.
   for(int offset:{14,24,19})if(event[offset+4]){order.resolved=target(event.data()+offset,order.point);break;}
   node.orders.push_back(order);
  }
  if(!node.orders.empty())raw.emplace(pointer,std::move(node));
 }
 auto orderFor=[&](uint32_t node,uint32_t actor)->const Order*{auto it=raw.find(node);if(it==raw.end())return nullptr;for(const auto&o:it->second.orders)if(o.actor==actor)return o.resolved?&o:nullptr;return nullptr;};
 if(graph.highlight){auto it=raw.find(graph.highlight);if(it!=raw.end())for(const auto&o:it->second.orders)graph.white.insert(o.actor);}
 else for(const auto&p:paths)if(p.second.selected)graph.white.insert(p.first);
 for(uint32_t pointer:nodePointers){auto it=raw.find(pointer);if(it==raw.end()||!it->second.orders.front().resolved)continue;const auto&first=it->second.orders.front();const auto&firstPath=paths.at(first.actor);
  Node node;node.pointer=pointer;node.owner=firstPath.owner;node.point=first.point;node.badge=first.badge;
  std::map<std::tuple<int,int,int>,size_t>groups;Segment loop;bool hasLoop=false;
  for(const auto&o:it->second.orders){node.members.push_back(o.actor);const auto&path=paths.at(o.actor);auto found=std::find(path.nodes.begin(),path.nodes.end(),pointer);size_t index=size_t(found-path.nodes.begin());
   auto choose=[&](Segment&segment,const Segment&candidate){if(segment.bright)segment=candidate;};
   if(index){uint32_t previous=path.nodes[index-1];if(previous!=path.nodes.back())if(const auto*source=orderFor(previous,o.actor)){Segment line{source->point,node.point,path.owner,path.pending!=-1&&int(index)>=path.pending};auto key=std::make_tuple(line.from.x,line.from.y,line.from.z);auto group=groups.find(key);if(group==groups.end()){groups[key]=node.incoming.size();node.incoming.push_back(line);}else choose(node.incoming[group->second],line);}}
   if(path.loop==int(index))if(const auto*source=orderFor(path.nodes.back(),o.actor)){Segment line{source->point,node.point,path.owner,path.loopChanged};if(hasLoop)choose(loop,line);else{loop=line;hasLoop=true;}}
  }
  // Marker brightness is based on incoming groups before adding a loop.
  auto firstIndex=std::find(firstPath.nodes.begin(),firstPath.nodes.end(),pointer)-firstPath.nodes.begin();
  node.bright=!node.incoming.empty()||(firstPath.pending!=-1&&firstIndex>=firstPath.pending);
  if(hasLoop&&!groups.count(std::make_tuple(loop.from.x,loop.from.y,loop.from.z))){loop.loop=true;node.incoming.push_back(loop);}
  graph.nodes.push_back(std::move(node));
 }
 for(uint32_t pointer:actorOrder){const auto&path=paths.at(pointer);const auto*first=orderFor(path.nodes.front(),pointer);if(!first)continue;Point point{},rawPoint{};if(!pointerPoint(pointer,point,true)||!read(pointer+0x9C,&rawPoint,sizeof rawPoint)||!nativefeedback::worldPoint(rawPoint))return false;
  Actor actor;actor.pointer=pointer;actor.owner=path.owner;actor.badge=first->badge;
  int mission=game.template get<int>(pointer+0xAC,-1);if(mission<0)mission=game.template get<int>(pointer+0xB4,-1);
  bool foot=game.kind(pointer)!=3,limbo=game.template get<uint8_t>(pointer+0x81)!=0;
  uint32_t destination=foot?game.template get<uint32_t>(pointer+0x5A4):0,aux=foot?game.template get<uint32_t>(pointer+0x5A0):0,attack=game.template get<uint32_t>(pointer+0x2B4);Point current=rawPoint;
  bool acquired=foot&&game.template get<uint8_t>(pointer+0x5D1)!=0,attackMove=foot&&game.template get<int>(pointer+0x5C4,-1)==29;
  if(mission!=5&&!(mission==11&&(!foot||(!destination&&!attack)))){
   uint32_t target=mission==1?attack:aux?aux:destination;
   if(!target||pointerPoint(target,current,true)){actor.hasCurrent=true;actor.current={point,current,path.owner,false};point=current;actor.currentBadge=attackMove&&!acquired?434:mission==11?433:0;}
  }
  if(limbo&&actor.hasCurrent){actor.markerOnly=true;actor.marker=current;actor.hasCurrent=false;}
  else if(acquired&&actor.hasCurrent){uint32_t mega=game.template get<uint32_t>(pointer+0x5C8);if(!mega)mega=game.template get<uint32_t>(pointer+0x5CC);Point target{};if(mega&&pointerPoint(mega,target,false)){actor.hasMega=true;actor.mega={current,target,path.owner,false};}}
  actor.hasConnection=!actor.markerOnly&&(!acquired||!actor.hasCurrent);
  actor.connection={point,first->point,path.owner,path.pending==0};graph.actors.push_back(actor);
 }
 if(!snapshot.stable())return false;out=std::move(graph);return true;
}
}}
