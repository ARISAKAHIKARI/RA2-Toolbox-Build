#include "../../external_reader/source/local_ui_task.h"
#include "../../external_reader/source/render_profile.h"
#include "../../external_reader/source/sidebar_indicators.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
using namespace external;
struct Memory {
 std::vector<uint8_t>bytes=std::vector<uint8_t>(0xC00000);size_t calls=0,lookupReads=0;uint32_t fail=0;
 static constexpr uint32_t font=0x40000,data=0x41000,table=0x100000,bits=0x130000,label=0x30000,message=0x32000,scheme=0x46000,super=0x56000,swType=0x57000,title=0x58000,factory=0x60000,type=0x62000,cell=0x90000,cells=0x200000;
 mainview::View view{0x20000,0x21000,0x22000,50,0,600,800,600,0,0,640,600};
 template<class T>void put(uint32_t at,const T&value){assert(uint64_t(at)+sizeof value<=bytes.size());std::memcpy(bytes.data()+at,&value,sizeof value);}
 bool read(uint32_t at,void*out,size_t n){++calls;if(at==table)++lookupReads;if(at==fail||uint64_t(at)+n>bytes.size())return false;std::memcpy(out,bytes.data()+at,n);return true;}
 auto reader(){return [this](uint32_t p,void*out,size_t n){return read(p,out,n);};}
 void text(uint32_t at,const std::u16string&s){std::memcpy(bytes.data()+at,s.c_str(),(s.size()+1)*2);}
 void vector(uint32_t at,uint32_t items,std::initializer_list<uint32_t>values){put(at+4,items);put(at+8,int(values.size()));put(at+16,int(values.size()));for(auto v:values){put(items,v);items+=4;}}
 void viewport(){const auto&v=view;put(0xA8B230,v.scenario);put(0xA83D4C,v.house);put(0x887324,v.tactical);put(0xA8ED84,v.frame);put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));put(0x8A00A4,v.screenW);put(0x8A00A8,v.screenH);put(v.tactical+0xB0,v.cameraX);put(v.tactical+0xB4,v.cameraY);int rect[]{v.x,v.y,v.width,v.height};put(0x886FA0,rect);}
 Memory(){
  viewport();put(0x89E7C0,104);put(0xAC13BC,416);put(0xB0CD48,.0866025403784);put(0x89C4D0,font);put(font+4,data);put(font+0x1C,5);
  const uint32_t fields[]{8,1,3,5,128,4,table,bits,128};put(data,fields);
  for(int ch=32;ch<=126;++ch){put(table+ch*2,uint16_t(ch-31));const uint8_t glyph[]{uint8_t(ch==32?2:3),0xA0,0x40,0xE0};put(bits+(ch-32)*4,glyph);}
  put(table+0x4E3B*2,uint16_t(96));put(table+0x8981*2,uint16_t(97));put(table+0xB0*2,uint16_t(98));const uint8_t chinese[]{5,0xF8,0x88,0xF8},degree[]{3,0xE0,0xA0,0xE0};put(bits+95*4,chinese);put(bits+96*4,chinese);put(bits+97*4,degree);
  put(0xB054D4,0x45000u);put(0xB054E0,6);for(int i=0;i<6;++i){put(0x45000+i*4,scheme+i*0x400);const uint8_t rgb[]{uint8_t(10+i*30),120,240};put(scheme+i*0x400+0x308,rgb);}
  const uint8_t rgb[]{255,123,45};put(view.house+0x56F9,rgb);
  const uint32_t csf[]{3,3,0x50000,0x51000};put(0xB1CF6C,csf);const char*names[]{"TXT_PRI","TXT_PRIMARY","MSG:BlackoutTimer"};const std::u16string values[]{u"PRI",u"主要",u"Blackout"};for(int i=0;i<3;++i){std::memcpy(bytes.data()+0x50000+i*40,names[i],std::strlen(names[i]));put(0x50000+i*40+32,1);put(0x50000+i*40+36,i);put(0x51000+i*4,0x52000u+i*256);text(0x52000+i*256,values[i]);}
 }
 void messages(){put(0xA8BC60,label);put(label,0x7F5B44u);put(label+0xC,40);put(label+0x10,12);put(label+0x30,message);put(label+0x38,-1);text(message,u"A 主要");}
 nativefeedback::Object building(){put(factory,0x7E3EBCu);put(factory+0x10,13u);put(factory+0x520,type);put(factory+0x21C,view.house);put(factory+0x6C,500);put(factory+0x74,uint8_t(1));put(factory+0x90,uint8_t(1));put(factory+0x83,uint8_t(1));put(factory+0x214,-1);put(factory+0x3D3,uint8_t(1));const int location[]{7680,6400,0};put(factory+0x9C,location);put(type+0xEF0,0);put(type+0xEB8,0x28);put(0x8192B8,2);put(0x819310,2);return {factory,13,type,view.house,{500,500,80,80},3};}
 void rally(){put(factory+0x218,cell);const int16_t xy[]{30,25};put(cell+0x24,xy);put(cell+0x11B,int8_t(2));put(cells+(25*512+30)*4,cell);const int bounds[]{32,64,0,0,32,64};put(mapbounds::Map+0xF4,bounds);put(mapbounds::Map+0x13C,cells);put(mapbounds::Map+0x140,512*512);}
};

struct Request {uint32_t pid=77,match=8;uint64_t generation=1;int width=640,height=600;struct{uint64_t token=3;}context;mainview::View view;presentation::Profile profile;};
struct Reader {
 static Memory*memory;static worldframe::Token latest;static std::atomic<unsigned>ended,liveWorld,selectedFlags,liveRoster,resourceReads,lastDemand;
 worldframe::Token pinned;uint32_t demand=worldframe::Base;bool complete=true;
 bool open(uint32_t){return true;}void close(){pinned.reset();}void begin(){}
 bool beginWorldFrame(uint32_t match,worldframe::Token token={}){pinned=token?token:latest;complete=true;lastDemand=demand;return pinned&&pinned->key.match==match;}
 void endWorldFrame(){pinned.reset();++ended;}worldframe::Token worldFrameToken()const{return pinned;}
 bool worldFrameComplete()const{return complete;}void markWorldIncomplete(){complete=false;}
 uint32_t worldDemand()const{return demand;}void setWorldDemand(uint32_t value){demand=value|worldframe::Base;}
 bool read(uint32_t p,void*out,size_t n){if(!n)return true;if(pinned&&pinned->read(p,out,n))return true;complete=false;return false;}
 bool readFresh(uint32_t p,void*out,size_t n){return read(p,out,n);}
 bool readOptional(uint32_t p,void*out,size_t n){if(!n)return true;if(pinned&&pinned->read(p,out,n))return true;if(!pinned||!pinned->observed(p,n))complete=false;return false;}
 bool readLive(uint32_t p,void*out,size_t n){
  if(p==Memory::factory+0x9C||p==Memory::factory+0x6C||p==Memory::factory+0x21C||p==Memory::factory+0xAC||p==0x820000||p==memory->view.house+0x53A4||p==memory->view.house+0x53A8)++liveWorld;
  if(p==Memory::factory+0x83)++selectedFlags;if(p>=0xA8EC78&&p<0xA8EC90)++liveRoster;
  if(p==Memory::table||p==Memory::bits||p==0x800000)++resourceReads;
  return memory->read(p,out,n);
 }
};
Memory*Reader::memory=nullptr;worldframe::Token Reader::latest;std::atomic<unsigned>Reader::ended{0},Reader::liveWorld{0},Reader::selectedFlags{0},Reader::liveRoster{0},Reader::resourceReads{0},Reader::lastDemand{0};
static worldframe::Token seal(Memory&m,uint32_t serial=1,bool missingObject=false){
 auto result=std::make_shared<worldframe::Snapshot>();worldframe::Key key;key.pid=77;key.owner=9;key.match=8;key.serial=serial;key.tick=1000;key.frame=m.view.frame;key.scenario=m.view.scenario;key.house=m.view.house;key.cells=Memory::cells;key.cellCount=512*512;key.demand=worldframe::Base|worldframe::Passengers;key.connectionLow=4;key.generationLow=2;
 result->reset(key);auto read=m.reader();
 for(uint32_t p=0x10000;p<m.bytes.size();p+=worldframe::PageSize){
  // Resource bytes and the live PlanMgr allocations never enter this sample.
  if((p>=0x40000&&p<0x45000)||(p>=0x50000&&p<0x54000)||(p>=0x100000&&p<0x132000)||(p>=0x600000&&p<0x680000)||(p>=0x800000&&p<0x810000)||(missingObject&&p==Memory::factory))continue;
  assert(result->prefetch(read,p,worldframe::PageSize));
 }
 assert(result->seal());return result;
}
template<class Predicate>void until(Predicate ready){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!ready()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
static void setup(Memory&m){
 auto object=m.building();m.put(Memory::factory+0x83,uint8_t(0));m.put(Memory::type,0x63000u);m.put(0x63000+0x74,0x716290u);m.put(Memory::type+0x3D4,2);m.put(Memory::type+0xA0,500);
 m.put(m.view.house+0x53A4,100);m.put(m.view.house+0x53A8,30);m.vector(0xA8EC78,0x54000,{object.pointer});m.vector(0xA8ECB8,0x55000,{});
 const int shifts[]{11,3,0,3,5,2};m.put(0x8A0DD0,shifts);
 constexpr uint32_t converter=0x810000,palette=0x820000,shape=0x800000;
 m.put(0x87F6C8,converter);m.put(converter,0x7E5358u);m.put(converter+4,2u);m.put(converter+0x16C,1);m.put(converter+0x174,palette);m.put(palette+2,uint16_t(0xF800));
 const uint16_t header[]{0,2,2,3};m.put(shape,header);for(int f=0;f<3;++f){m.put(shape+8+f*24+4,uint16_t(2));m.put(shape+8+f*24+6,uint16_t(2));m.put(shape+8+f*24+20,uint32_t(80+f*4));const uint8_t pixels[]{1,1,1,1};m.put(shape+80+f*4,pixels);}
 const int bounds[]{64,64,10,10,1,1};m.put(mapbounds::Map+0xF4,bounds);m.put(mapbounds::Map+0x13C,Memory::cells);m.put(mapbounds::Map+0x140,512*512);m.put(Memory::cells+(64*512+64)*4,Memory::cell);const int16_t xy[]{64,64};m.put(Memory::cell+0x24,xy);
}
static void selectedAndResources(Memory&m,const worldframe::Token&token){
 Reader reader;assert(reader.beginWorldFrame(8,token));worldui::WorldRead<Reader>world{&reader};auto live=[&](uint32_t p,void*out,size_t n){return reader.readLive(p,out,n);};
 auto object=nativefeedback::Object{Memory::factory,13,Memory::type,m.view.house,{100,100,40,40},3};std::vector<nativefeedback::Object>objects{object};
 m.vector(0xA8ECB8,0x55000,{Memory::factory});m.put(Memory::factory+0x83,uint8_t(0)); // Selected vector is the UI source, not a stale actor flag.
 m.put(Memory::factory+0x9C,nativefeedback::Point3{999,888,777});m.put(Memory::factory+0x6C,1);m.put(Memory::factory+0x21C,m.view.house+4);
 m.put(0x820002,uint16_t(0x07E0));
 worldui::Read read(world,live);assert(read.begin(objects));std::vector<nativefeedback::Selected>selected;
 assert(nativefeedback::selected(read,objects,selected)&&selected.size()==1&&selected[0].health==500&&selected[0].strength==500);
 std::array<uint8_t,0x530>block{};assert(read(Memory::factory,block.data(),block.size()));assert(block[0x83]==1&&nativefeedback::u32(block.data()+0x6C)==500&&nativefeedback::u32(block.data()+0x21C)==m.view.house);
 int xyz[3]{};std::memcpy(xyz,block.data()+0x9C,12);assert(xyz[0]==7680&&xyz[1]==6400&&xyz[2]==0);
 localui::BitmapFont font;assert(font.begin(read));int width=0;assert(font.measure(read,u"A 主要",width)&&width==19);
 liveshape::Cache shapes;shapes.begin();auto image=shapes.get(read,0x800000,0,0x87F6C8);assert(image&&image->pixels[0]==0xFFFF0000u); // Palette stays sealed while SHP bytes are resource reads.
 assert(read.stable()&&reader.complete&&Reader::resourceReads>0&&Reader::liveWorld==0&&Reader::selectedFlags==0&&Reader::liveRoster==0);
 m.vector(0xA8ECB8,0x55000,{});assert(!read.stable());worldui::Read unselected(world,live);assert(unselected.begin(objects)&&nativefeedback::selected(unselected,objects,selected)&&selected.empty()&&unselected.stable());
 m.vector(0xA8ECB8,0x55000,{Memory::factory});m.put(Memory::factory+0x10,14u);worldui::Read recycled(world,live);assert(!recycled.begin(objects));m.put(Memory::factory+0x10,13u);
}
static void planningInput(Memory&m,const worldframe::Token&token){
 constexpr uint32_t actor=0x700000,target=0x710000,node=0x600000,node2=0x601000,member=0x602000,event=0x603000,handle=0x604000;
 Reader reader;assert(reader.beginWorldFrame(8,token));worldui::WorldRead<Reader>world{&reader};auto live=[&](uint32_t p,void*out,size_t n){return reader.readLive(p,out,n);};
 m.put(0xAC4CF4,uint8_t(1));m.put(0xAC4CCC,node);m.vector(0xAC4B30,0x605000,{node});m.vector(0xAC4C78,0x606000,{handle});m.put(handle,actor);m.vector(handle+4,0x607000,{node});m.put(handle+0x8C,0);m.put(handle+0x94,-1);m.put(actor+0x514,handle);m.put(actor+0x83,uint8_t(1));
 m.vector(node,0x608000,{member});m.put(member,actor);m.put(member+4,event);m.put(event,uint8_t(4));m.put(event+7,91u);m.put(event+11,uint8_t(0x34));m.put(event+14,92u);m.put(event+18,uint8_t(0x34));
 m.put(actor+0x9C,nativefeedback::Point3{999,888,777});m.put(actor+0x21C,m.view.house+4);m.put(actor+0xAC,1);
 planning::Graph graph;assert(planning::capture(world,live,m.view.house,graph)&&graph.active&&graph.nodes.size()==1&&graph.actors.size()==1);
 assert(graph.nodes[0].point.x==9000&&graph.nodes[0].point.y==7000&&graph.actors[0].connection.from.x==7000&&graph.actors[0].connection.from.y==6000&&!graph.actors[0].hasCurrent&&graph.actors[0].owner==m.view.house);
 // Paused logic frame: a newly allocated UI node appears immediately.
 m.vector(0xAC4B30,0x605000,{node,node2});m.vector(handle+4,0x607000,{node,node2});m.vector(node2,0x609000,{member});m.put(0xAC4CCC,node2);
 assert(planning::capture(world,live,m.view.house,graph)&&graph.nodes.size()==2&&graph.hover==node2&&reader.complete);
 unsigned hoverReads=0;auto tearing=[&](uint32_t p,void*out,size_t n){if(!live(p,out,n))return false;if(p==0xAC4CCC&&++hoverReads==2){uint32_t changed=0;std::memcpy(out,&changed,4);}return true;};
 assert(!planning::capture(world,tearing,m.view.house,graph)&&graph.nodes.empty());m.put(0xAC4CF4,uint8_t(0));
}
static void placement(Memory&m,const worldframe::Token&token){
 Reader reader;assert(reader.beginWorldFrame(8,token));worldui::WorldRead<Reader>world{&reader};auto live=[&](uint32_t p,void*out,size_t n){return reader.readLive(p,out,n);};
 auto view=m.view;view.cameraX=-100;view.cameraY=1875;constexpr uint32_t map=nativefeedback::Map;
 m.put(map+0x1174,int16_t(64));m.put(map+0x1176,int16_t(64));m.put(map+0x117C,0x610000u);m.put(map+0x11A8,Memory::type);m.put(0x8A03FC,0x800000u);const int16_t offsets[]{0,0,0x7fff,0x7fff};m.put(0x610000,offsets);
 m.put(Memory::cell+0x12C,uint8_t(2));m.put(Memory::cell+0x11B,int8_t(30));
 nativefeedback::FoundationStencil stencil;std::vector<nativefeedback::Rect>holes;assert(stencil.capture(world,live,view,640,600,holes)&&!holes.empty());auto original=holes;
 m.put(Memory::cell+0x11B,int8_t(40));assert(stencil.capture(world,live,view,640,600,holes)&&holes.size()==original.size()&&std::equal(holes.begin(),holes.end(),original.begin(),[](const auto&a,const auto&b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}));
 m.put(Memory::cell+0x12C,uint8_t(0));assert(stencil.capture(world,live,view,640,600,holes)&&holes.empty()&&reader.complete);m.put(map+0x11A8,0u);
}
static void sidebar(Memory&m,const worldframe::Token&token){
 Reader reader;assert(reader.beginWorldFrame(8,token));worldui::WorldRead<Reader>world{&reader};auto live=[&](uint32_t p,void*out,size_t n){return reader.readLive(p,out,n);};
 m.put(m.view.house+0x53A4,999);m.put(m.view.house+0x53A8,888);sidebarindicators::Sample sample;
 assert(sidebarindicators::capture(world,m.view.house,token->key.frame,sample,false)&&reader.complete&&sample.output==100&&sample.drain==30&&sample.buildings&&sample.total==0);
 sidebarindicators::Engine engine;nativefeedback::Rect bounds;std::shared_ptr<const std::vector<uint32_t>>pixels;
 assert(engine.render(live,m.view,sample,800,600,false,true,bounds,pixels)&&pixels&&bounds.valid()&&Reader::liveWorld==0);
}
static void taskPublication(Memory&m,const worldframe::Token&token){
 Reader::latest=token;std::atomic<uint32_t>now{5000};localui::Task<Request,Reader>task([&]{return now.load();});Request request;request.view=m.view;
 const nativefeedback::Object object{Memory::factory,13,Memory::type,m.view.house,{500,500,80,80},3};m.vector(0xA8ECB8,0x55000,{Memory::factory});
 task.submit(request,{object},token);until([&]{return bool(task.get());});auto first=task.get();assert(first->sourceFrame==token&&first->primary==1&&first->images[2].pixels&&first->sourceFrame->key.tick==1000);
 assert(Reader::lastDemand==(worldframe::Base|worldframe::Passengers)&&Reader::liveWorld==0&&Reader::selectedFlags==0&&Reader::liveRoster==0);
 m.vector(0xA8ECB8,0x55000,{});task.submit(request,{object},token);until([&]{return task.get()!=first;});auto unselected=task.get();assert(unselected->primary==0&&!unselected->images[2].pixels&&unselected->sourceFrame==token);
 m.vector(0xA8ECB8,0x55000,{Memory::factory});task.submit(request,{object},token);until([&]{return task.get()!=unselected;});auto stable=task.get();assert(stable->primary==1&&task.visible(request,now,token)==stable);
 // A missing critical raw object never reads the current game object and
 // never replaces a complete result with an apparently empty UI layer.
 auto incomplete=seal(m,2,true);unsigned done=Reader::ended;task.submit(request,{object},incomplete);until([&]{return Reader::ended>done;});assert(task.get()==stable&&Reader::liveWorld==0&&!task.visible(request,now,incomplete)&&task.visible(request,now,token)==stable);
 // An unavailable UI/resource producer also retains the complete prior UI.
 m.fail=0x89C4D0;done=Reader::ended;task.submit(request,{object},token);until([&]{return Reader::ended>done;});assert(task.get()==stable);m.fail=0;
 // No Runtime token cannot be replaced by a native-world fallback.
 Reader::latest.reset();done=Reader::ended;task.submit(request,{object});until([&]{return Reader::ended>done;});assert(task.get()==stable&&Reader::liveWorld==0);
 task.shutdown();
}
int main(){
 Memory memory;Reader::memory=&memory;setup(memory);
 constexpr uint32_t actor=0x700000,target=0x710000;
 for(auto [p,id,point]:{std::tuple<uint32_t,uint32_t,nativefeedback::Point3>{actor,91,{7000,6000,0}},{target,92,{9000,7000,0}}}){memory.put(p,0x7F5C70u);memory.put(p+0x10,id);memory.put(p+0x21C,memory.view.house);memory.put(p+0x9C,point);memory.put(p+0xAC,5);}
 memory.vector(0xA8EC78,0x54000,{Memory::factory,actor,target});auto token=seal(memory);Reader::latest=token;
 selectedAndResources(memory,token);planningInput(memory,token);placement(memory,token);sidebar(memory,token);taskPublication(memory,token);
 std::cout<<"PASS sealed UI sampling: actual selection/HP and actor blocks, zero live world/selected-flag/Techno-roster reads, live selected-set races and pointer reuse rejected, live font/SHP with sealed palette, paused new planning nodes with sealed actor state, live planning race rejection, live placement over sealed cells, sealed sidebar power with resource-only font rendering, actual asynchronous Task keeps same Token and old tick, strict missing/resource/unavailable publication retention; no Windows or game visual claim\n";
}
