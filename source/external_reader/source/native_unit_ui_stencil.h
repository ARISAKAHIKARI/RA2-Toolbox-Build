#pragma once
#include "native_effect_stencil.h"
#include "native_pip_numeric.h"
#include "building_animations.h"
// Native pips are UI drawn after the unit body, not another world actor.
// Verified state produces SHP commands at native draw anchors. The local UI
// draws their color; the old alpha-only adapter remains for legacy callers.
namespace external { namespace nativeeffects {
class UnitUIStencil {
 struct Mask {std::array<uint8_t,24> header{};int x=0,y=0;std::vector<nativefeedback::Rect> runs;};
 uint32_t shape_=0;std::array<uint8_t,8> header_{};std::map<unsigned,Mask> masks_;
 struct Unit {
  uint32_t pointer=0,id=0,type=0,owner=0,vt=0;nativefeedback::Point3 point;
  int ammo=0,count=0;uint32_t passenger=0;bool operator==(const Unit&o)const{
   return pointer==o.pointer&&id==o.id&&type==o.type&&owner==o.owner&&vt==o.vt&&ammo==o.ammo&&count==o.count&&passenger==o.passenger&&
    point.x==o.point.x&&point.y==o.point.y&&point.z==o.point.z;
  }
 };
 template<class Read>bool unit(Read read,const nativefeedback::Object&o,uint32_t house,Unit&v){
  using namespace nativefeedback;std::array<uint8_t,0x300> data{};uint32_t type=0;
  if(!range(o.pointer,data.size())||!read(o.pointer,data.data(),data.size()))return false;
  auto vt=u32(data.data());if(vt!=0x7F5C70&&vt!=0x7E22A4&&vt!=0x7EB058&&vt!=0x7E3EBC)return false;
  if(!read(o.pointer+(vt==0x7E3EBC?0x520:vt==0x7EB058?0x6C0:0x6C4),&type,4)||type!=o.type||!range(type,0xE00)||
     u32(data.data()+0x10)!=o.id||!o.id||u32(data.data()+0x21C)!=o.owner||
     data[0x81]||data[0x90]!=1||int32_t(u32(data.data()+0x6C))<=0)return false;
  v={o.pointer,o.id,type,o.owner,vt};std::memcpy(&v.point,data.data()+0x9C,sizeof v.point);
  v.ammo=int32_t(u32(data.data()+0x2FC));v.count=int32_t(u32(data.data()+0x114));v.passenger=u32(data.data()+0x118);
  return worldPoint(v.point)&&v.ammo>=-1&&v.ammo<=1000000&&v.count>=0&&v.count<=512;
 }
 template<class Read>bool mask(Read read,uint32_t pointer,unsigned frame,const Mask*&out){
  using namespace nativefeedback;std::array<uint8_t,8> header{};
  if(!range(pointer,16)||!read(pointer,header.data(),8))return false;
  if(u16(header.data())==0xffff){uint32_t resolved=0;if(!read(pointer+12,&resolved,4)||!range(resolved,8)||!read(resolved,header.data(),8))return false;pointer=resolved;}
  if(u16(header.data())||frame>=u16(header.data()+6))return false;
  if(shape_!=pointer||header_!=header){clear();shape_=pointer;header_=header;}
  std::array<uint8_t,24> fh{};if(!range(pointer,8+size_t(frame+1)*24)||!read(pointer+8+frame*24,fh.data(),fh.size()))return false;
  auto found=masks_.find(frame);if(found!=masks_.end()&&found->second.header==fh){out=&found->second;return true;}
  std::array<uint32_t,256> palette;palette.fill(0xffffffff);palette[0]=0;voxel::Image image;
  if(!voxel::sprite(read,pointer,palette,image,int(frame),nullptr,255)||image.width>128||image.height>128)return false;
  Mask value;value.header=fh;value.x=image.x;value.y=image.y;
  for(int y=0;y<image.height;++y)for(int x=0;x<image.width;){if(!(image.pixels[size_t(y)*image.width+x]>>24)){++x;continue;}int left=x++;
   while(x<image.width&&(image.pixels[size_t(y)*image.width+x]>>24))++x;value.runs.push_back({left,y,x-left,1});}
  if(masks_.size()>=128)masks_.clear();out=&masks_.insert_or_assign(frame,std::move(value)).first->second;return true;
 }
 // 473460 sums each passenger's Size with an integer truncation each step;
 // 709E65 fills the slots in reverse linked-list order. Do not replace an IFV
 // passenger's own pip frame with a guessed generic passenger count.
 template<class Read>bool passengerFrames(Read read,const Unit&u,int maximum,std::vector<unsigned>&frames,bool absorber=false){
  using namespace nativefeedback;struct Passenger{uint32_t p=0,id=0,next=0,type=0,vt=0;double size=0;int fillers=0,typeOffset=0;unsigned frame=0;};
  std::vector<Passenger> passengers;std::vector<uint32_t> seen;uint32_t p=u.passenger;int total=0;
  for(int n=0;n<u.count;++n){
   if(!range(p,0x6C8)||std::find(seen.begin(),seen.end(),p)!=seen.end())return false;seen.push_back(p);
   Passenger q;q.p=p;uint32_t vt=0,parent=0;double size=0;
   if(!read(p,&vt,4)||!read(p+0x10,&q.id,4)||!q.id||!read(p+0x30,&q.next,4)||!read(p+0x11C,&parent,4)||parent!=u.pointer)return false;
   q.vt=vt;q.typeOffset=vt==0x7EB058?0x6C0:0x6C4;
   if(vt==0x7EB058){if(!read(p+q.typeOffset,&q.type,4)||!range(q.type,0xE00)||!read(q.type+0xDFC,&q.frame,4))return false;}
   else if(vt==0x7F5C70||vt==0x7E22A4){if(!read(p+q.typeOffset,&q.type,4))return false;q.frame=vt==0x7F5C70?5:1;}
   else return false;
   if(q.frame>=8192||(!absorber&&(!range(q.type,0x388)||!read(q.type+0x380,&size,8)||!std::isfinite(size)||size<=0||size>512)))return false;
   q.size=size;q.fillers=absorber||vt==0x7E22A4?0:std::max(0,int(size)-1);if(absorber&&vt==0x7F5C70)q.frame=4;total+=absorber?1:int(size);if(total>8192)return false;passengers.push_back(q);p=q.next;
  }
  if(p)return false;
  frames.assign(size_t(maximum),0);if(total>maximum){frames.clear();return true;}
  int slot=absorber?0:total-1;for(const auto&q:passengers){for(int extra=0;extra<q.fillers&&slot>=0;++extra)frames[size_t(slot--)]=3;
   if(slot>=0&&slot<maximum){frames[size_t(slot)]=q.frame;slot+=absorber?1:-1;}
   uint32_t id=0,next=0,type=0,vt=0,parent=0;double size=0;unsigned pip=0;
   if(!read(q.p,&vt,4)||vt!=q.vt||!read(q.p+0x10,&id,4)||id!=q.id||!read(q.p+0x30,&next,4)||next!=q.next||
      !read(q.p+q.typeOffset,&type,4)||type!=q.type||!read(q.p+0x11C,&parent,4)||parent!=u.pointer||
      (!absorber&&(!read(type+0x380,&size,8)||size!=q.size))||(vt==0x7EB058&&(!read(type+0xDFC,&pip,4)||pip!=q.frame)))return false;
  }
  return true;
 }
public:
 void clear(){shape_=0;header_={};masks_.clear();}
 struct Command {uint32_t shape=0;unsigned frame=0;int x=0,y=0;};
 template<class Read>bool commands(Read read,const mainview::View&view,const std::vector<nativefeedback::Object>&objects,std::vector<Command>&out){
  using namespace nativefeedback;
  if(!view.valid()||objects.size()>8192)return false;
  std::array<uint32_t,6> head{},after{};uint32_t house=0;int frame=0;double zscale=0;
  if(!read(0xA8ECB8,head.data(),sizeof head)||head[2]>8192||head[4]>head[2]||head[4]>8192||
     (head[4]&&!range(head[1],size_t(head[4])*4)))return false;
  if(!read(0xA83D4C,&house,4)||!range(house,4))return false;
  std::vector<uint32_t> selected(head[4]);if(!read(head[1],selected.data(),selected.size()*4))return false;
  auto sorted=selected;std::sort(sorted.begin(),sorted.end());if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())return false;
  std::vector<Command>pending;uint32_t shape=0;bool sampled=false;
  auto emit=[&](int x,int y,unsigned f){
   if(!shape&&(!read(0xAC1480,&shape,4)||!range(shape,16)))return false;
   if(pending.size()>=131072)return false;pending.push_back({shape,f,x,y});return true;
  };
  GameView game(read);
  for(const auto&o:objects){
   bool chosen=std::binary_search(sorted.begin(),sorted.end(),o.pointer);uint8_t hover=0,sinking=0;if(!read(o.pointer+0x431,&hover,1)||!read(o.pointer+0x3CD,&sinking,1))continue;if((!chosen&&!hover)||sinking)continue;
   uint32_t vt=0;if(!read(o.pointer,&vt,4))return false;if(vt!=0x7F5C70&&vt!=0x7E22A4&&vt!=0x7EB058&&vt!=0x7E3EBC)continue;
   Unit u;if(!unit(read,o,house,u))return false;
   const bool building=vt==0x7E3EBC;int asking=game.template get<int>(house+0x30,-1);
   bool ally=o.owner==house||(asking>=0&&asking<32&&(game.template get<uint32_t>(o.owner+0x5788)&(1u<<asking)));
   if(!ally&&!(building&&(game.template get<uint8_t>(u.type+0x157B)||game.template get<uint8_t>(u.type+0x3D8))))continue;
   int mode=0,capacity=0,maximum=0,wrap=0;uint32_t typeVT=0,pipMax=0;
   if(!read(u.type,&typeVT,4)||!range(typeVT,0x78)||!read(typeVT+0x74,&pipMax,4)||pipMax!=0x716290||
      !read(u.type+0x3D4,&mode,4)||!read(u.type+0x5E0,&capacity,4)||!read(u.type+0x3E4,&wrap,4))return false;
   if(building&&mode==2)continue;
   if(mode==2)maximum=vt==0x7EB058?3:5;
   if(mode==5){uint32_t weapon=0;if(!read(u.type+0x898,&weapon,4)||!range(weapon,0xA8)||!read(weapon+0xA4,&maximum,4))return false;}
   else if(mode!=2&&!read(u.type+(mode==1?0x684:0x5E0),&maximum,4))return false;
   if(maximum<0||maximum>512||capacity<0||capacity>512||wrap<0||wrap>512)return false;
   if(!sampled){if(!read(0xA8ED84,&frame,4)||frame<0||!read(0xB0CD48,&zscale,8)||!std::isfinite(zscale)||zscale<=0||zscale>1)return false;sampled=true;}
   int x=int((int64_t(u.point.x)-u.point.y)*30/256)-view.cameraX-15+(vt==0x7EB058?11:0);
   int delta=game.template get<int>(u.type+0x3E0);if(std::abs(int64_t(delta))>8192)return false;
   int y=int((int64_t(u.point.x)+u.point.y)*15/256)-screenHeight(u.point.z,zscale)-view.cameraY+10+delta;
   if(building){int w=0,h=0;if(!animations::foundation(read,u.type,w,h))return false;int left=-w*128,back=h*128;
    int cx=u.point.x+(w-1)*128,cy=u.point.y+(h-1)*128;x=int((int64_t(cx)-cy)*30/256)-view.cameraX+(left-back)*30/256+6;y=int((int64_t(cx)+cy)*15/256)-screenHeight(u.point.z,zscale)-view.cameraY+(left+back)*15/256-1;
   }
   if(!read(building?0xAC147C:0xAC1480,&shape,4)||!range(shape,16))return false;
   auto slot=[&](int i,unsigned f,int dx=0,int dy=0){return emit(x+i*4+dx,y+(building?i*2:0)+dy,f);};
   // Spawn row709B73 and garrison row709C27 precede the independent cargo
   // or PipScale row. They restart at the same origin, not a guessed new line.
   int spawnMax=0;if(!read(u.type+0xD5C,&spawnMax,4)||spawnMax<0||spawnMax>512)return false;
   if(spawnMax){uint32_t manager=0;if(!read(u.pointer+0x2D0,&manager,4))return false;if(manager){std::vector<uint32_t>nodes,afterNodes;uint32_t owner=0;if(!read(manager+0x24,&owner,4)||owner!=u.pointer||!game.vecChecked(manager+0x38,nodes,512))return false;int docked=0;for(auto node:nodes){int state=-1;if(!range(node,8)||!read(node+4,&state,4)||state<0||state>7)return false;docked+=state==0||state==6;}if(!game.vecChecked(manager+0x38,afterNodes,512)||afterNodes!=nodes)return false;for(int i=0;i<spawnMax;++i)if(!slot(i,unsigned(i<docked)))return false;}}
   if(building&&game.template get<uint8_t>(u.type+0x1584)){int limit=game.template get<int>(u.type+0x1580,-1);std::vector<uint32_t>occupants,afterOccupants;if(limit<0||limit>512||!game.vecChecked(u.pointer+0x684,occupants,512)||occupants.size()>size_t(limit))return false;for(int i=0;i<limit;++i){unsigned pip=6;if(size_t(i)<occupants.size()){uint32_t occupant=occupants[size_t(i)],type=0;if(game.kind(occupant)!=1||!read(occupant+0x6C0,&type,4)||!read(type+0xE00,&pip,4)||pip>8191)return false;}if(!slot(i,pip))return false;}if(!game.vecChecked(u.pointer+0x684,afterOccupants,512)||afterOccupants!=occupants)return false;}
   if(capacity){
    uint8_t gap=0;if(!read(u.type+0x805,&gap,1)||gap>1)return false;
    bool absorber=building&&(game.template get<uint8_t>(u.type+0x16AE)||game.template get<uint8_t>(u.type+0x16AF));
    std::vector<unsigned> frames;if(!passengerFrames(read,u,maximum,frames,absorber))return false;
    for(size_t i=0;i<frames.size();++i)if(!slot(int(i),frames[i],gap&&i?8:0))return false;
   }else if(mode==5){
    uint32_t manager=0;if(!read(u.pointer+0x2BC,&manager,4))return false;int count=0,limit=0,overload=0;uint8_t infinite=0;
    if(manager){uint32_t owner=0;std::vector<uint32_t>nodes;if(!read(manager+0x48,&owner,4)||owner!=u.pointer||!game.vecChecked(manager+0x24,nodes,8192)||!read(manager+0x3C,&limit,4)||!read(manager+0x40,&infinite,1)||!read(manager+0x44,&overload,4))return false;count=int(nodes.size());}
    for(int i=0;i<maximum;++i)if(!slot(i,unsigned(i<count)))return false;
    if(infinite&&count>limit&&!slot(maximum,overload>0?3:4))return false;
   }else if(mode==2){
    std::array<float,4>storage{},afterStorage{};int capacity=0,afterCapacity=0;
    if(!read(u.pointer+0x33C,storage.data(),sizeof storage)||!read(u.type+0x800,&capacity,4)||capacity<=0||capacity>1000000)return false;
    for(float value:storage)if(!std::isfinite(value)||value<0||value>1000000||std::floor(value)!=value)return false;
    unsigned gems=std::min(unsigned(maximum),unsigned(nativepips::quantity(uint32_t(storage[1]),capacity,maximum)));
    unsigned ore=unsigned(nativepips::quantity(uint32_t((storage[3]+storage[2])+storage[0]),capacity,maximum));ore=std::min(ore,unsigned(maximum)-gems);
    for(unsigned i=0;i<unsigned(maximum);++i)if(!slot(int(i),i<gems?5:i<gems+ore?2:0))return false;
    if(!read(u.pointer+0x33C,afterStorage.data(),sizeof afterStorage)||afterStorage!=storage||!read(u.type+0x800,&afterCapacity,4)||afterCapacity!=capacity)return false;
   }else if(mode==1){
    if(!wrap){for(int i=0;i<std::min(u.ammo,maximum);++i){
     // Vera20K ui_overlays::scale_pip_draws / native70A345..41C:
     // the ordinary Ammo arm uses frame13, without an empty outline.
     if(!slot(i,13,0,-3))return false;
    }}else for(int i=0;i<wrap;++i){int layer=maximum/wrap-1;while(layer>=0&&layer*wrap+i>=u.ammo)--layer;
     if(!slot(i,unsigned(layer<0?14:15+layer),0,-3))return false;
    }
   }
   int modeAfter=0,capacityAfter=0,wrapAfter=0,maximumAfter=maximum;
   if(!read(u.type+0x3D4,&modeAfter,4)||modeAfter!=mode||!read(u.type+0x5E0,&capacityAfter,4)||capacityAfter!=capacity||!read(u.type+0x3E4,&wrapAfter,4)||wrapAfter!=wrap||
      (mode!=2&&mode!=5&&(!read(u.type+(mode==1?0x684:0x5E0),&maximumAfter,4)||maximumAfter!=maximum)))return false;
   Unit verify;if(!unit(read,o,house,verify)||!(u==verify))return false;
  }
  int frameAfter=0;uint32_t houseAfter=0,shapeAfter=0;
  if(!read(0xA8ECB8,after.data(),sizeof after)||after!=head||(!sorted.empty()&&!read(head[1],sorted.data(),sorted.size()*4))||sorted!=selected||
     !read(0xA83D4C,&houseAfter,4)||houseAfter!=house||(sampled&&(!read(0xA8ED84,&frameAfter,4)||frameAfter!=frame))||
     (shape&&!range(shape,16)))return false;
  if(out.size()+pending.size()>131072)return false;out.insert(out.end(),pending.begin(),pending.end());return true;
 }
 // Legacy native passthrough adapter. Production UI consumes commands and
 // renders their actual palette; this mask remains for compatibility callers.
 template<class Read>bool capture(Read read,const mainview::View&view,int width,int height,const std::vector<nativefeedback::Object>&objects,std::vector<nativefeedback::Rect>&out){
  using namespace nativefeedback;if(width<1||height<1||width>8192||height>8192)return false;
  std::vector<Command>draws;if(!commands(read,view,objects,draws))return false;std::vector<Rect>pending;
  const double sx=double(width)/view.width,sy=double(height)/view.height;if(sx>16||sy>16)return false;
  for(const auto&c:draws)for(unsigned frame:std::array<unsigned,2>{{c.frame,c.frame==13?12u:c.frame}}){
   const Mask*m=nullptr;if(!mask(read,c.shape,frame,m))return false;
   for(auto run:m->runs){int l=int(std::floor((c.x+m->x+run.x)*sx)),r=int(std::ceil((c.x+m->x+run.x+run.w)*sx));int t=int(std::floor((c.y+m->y+run.y)*sy)),b=int(std::ceil((c.y+m->y+run.y+1)*sy));auto rect=intersect({l,t,r-l,b-t},{0,0,width,height});
    for(int y=rect.y;rect.valid()&&y<rect.y+rect.h;++y){if(pending.size()>=131072)return false;pending.push_back({rect.x,y,rect.w,1});}
   }if(c.frame!=13)break;
  }
  if(out.size()+pending.size()>131072)return false;out.insert(out.end(),pending.begin(),pending.end());return true;
 }

};
}}
