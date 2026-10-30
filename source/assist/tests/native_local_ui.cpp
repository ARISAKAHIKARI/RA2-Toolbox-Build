#include "../../external_reader/source/local_ui_task.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <iostream>
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
struct Request {uint32_t pid=77,match=8;uint64_t generation=1;int width=640,height=600;struct{uint64_t token=3;}context;mainview::View view;};
struct Reader {
 static Memory*memory;static std::mutex mutex;static std::condition_variable cv;static bool block,entered;
 bool open(uint32_t){return true;}void close(){}void begin(){}
 bool readFresh(uint32_t p,void*out,size_t n){return memory->read(p,out,n);}
 bool read(uint32_t p,void*out,size_t n){if(p==0x89C4D0){std::unique_lock<std::mutex>lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[]{return !block;});}return readFresh(p,out,n);}
};
Memory*Reader::memory=nullptr;std::mutex Reader::mutex;std::condition_variable Reader::cv;bool Reader::block=false,Reader::entered=false;
template<class Predicate>void until(Predicate ready){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!ready()){assert(std::chrono::steady_clock::now()<end);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(){
 Memory m;auto read=m.reader();localui::BitmapFont font;assert(font.begin(read));int width=0;assert(font.measure(read,u"A 主要",width)&&width==19);auto*g=font.glyph(read,'A');assert(g&&g->width==3&&g->ink==std::vector<uint8_t>({1,0,1,0,1,0,1,1,1}));
 auto decoded=font.decodes();assert(font.begin(read)&&font.measure(read,u"A 主要",width)&&font.decodes()==decoded&&m.lookupReads==1);auto*missing=font.glyph(read,0x9999);assert(missing&&missing->missing&&missing->ink[4]==1);auto*space=font.glyph(read,' ');assert(space&&!space->missing&&std::count(space->ink.begin(),space->ink.end(),1)==0);
 localui::Plan textPlan;localui::Painter paint(textPlan,640,600,640,600);assert(paint.text(font,read,u"A ",40,12,0xFFFF0000));nativefeedback::Canvas image;nativefeedback::Rect bounds;assert(nativefeedback::paintInteractions(textPlan.layers[0],{},image,bounds));assert(bounds.x==40&&bounds.y==12&&bounds.w==4&&bounds.h==4);assert(image.pixels[0]==0xFFFF0000&&image.pixels[1]==0&&image.pixels[15]==0xFF000000);
 m.put(Memory::font+0x1C,6);assert(!font.stable(read)&&font.begin(read)&&font.lineHeight()==6&&m.lookupReads==2);m.put(Memory::data+4,0u);assert(!font.begin(read));m.put(Memory::data+4,1u);m.put(Memory::font+0x1C,5);
 // Reads at page edges may fail as a block even though the terminated string is valid.
 std::u16string value;auto page=[&](uint32_t p,void*out,size_t n){return n>2?false:read(p,out,n);};m.text(Memory::message,u"A");assert(localui::string(page,Memory::message,value)&&value==u"A");
 localui::Engine engine;localui::Plan plan;assert(engine.capture(read,m.view,{},640,600,plan)&&plan.messages==0&&plan.timers==0);
 m.messages();assert(engine.capture(read,m.view,{},640,600,plan)&&plan.messages==1&&!plan.layers[0].empty()&&plan.layers[1].empty());auto before=plan.layers[0];assert(engine.capture(read,m.view,{},640,600,plan)&&plan.layers[0]==before);m.put(Memory::label+0x40,uint8_t(1));assert(engine.capture(read,m.view,{},640,600,plan)&&plan.messages==0);m.put(Memory::label+0x40,uint8_t(0));
 m.put(Memory::label+4,Memory::label);assert(!engine.capture(read,m.view,{},640,600,plan)&&plan.layers[0].empty());m.put(Memory::label+4,0u);
 m.put(0xA8BC60,0u);m.put(0xA8BC60+0x19,uint8_t(1));m.put(0xA8BC60+0x24,Memory::label);assert(engine.capture(read,m.view,{},640,600,plan)&&plan.messages==1);m.put(0xA8BC60+0x19,uint8_t(0));
 m.vector(0xA83D50,0x55000,{Memory::super});m.put(Memory::super,0x7F3FE8u);m.put(Memory::super+0x28,Memory::swType);m.put(Memory::super+0x2C,m.view.house);m.put(Memory::super+0x30,10);m.put(Memory::super+0x38,600);m.put(Memory::swType+0x60,Memory::title);m.text(Memory::title,u"Nuclear");
 m.put(m.view.scenario+0x11E8,5);m.put(m.view.scenario+0x11F0,150);m.put(m.view.scenario+0x11F4,Memory::title);m.vector(0xA80228,0x59000,{m.view.house});m.put(m.view.house+0x2A4,40);m.put(m.view.house+0x2AC,450);
 assert(engine.capture(read,m.view,{},640,600,plan)&&plan.timers==3);assert(localui::clockText(15*3661)==u"1:01:01"&&localui::clockText(14)==u"00:00"&&localui::remaining(-1,600,999)==600&&localui::remaining(10,600,1000)==0);
 auto object=m.building();m.rally();m.messages();assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.messages==1&&plan.timers==3&&plan.primary==1&&plan.rallies==1);
 for(size_t i=0;i<4;++i){assert(!plan.layers[i].empty());assert(nativefeedback::paintInteractions(plan.layers[i],{},image,bounds));if(i==2)assert(bounds.x==140&&bounds.y==208&&bounds.w==20&&bounds.h==9);}
 // A UI anchor comes from the current object location, never its cached body rectangle.
 auto primary=plan.layers[2];object.bounds={1,1,1,1};assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.layers[2]==primary);m.put(Memory::factory+0x83,uint8_t(0));assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.primary==0&&plan.rallies==0);m.put(Memory::factory+0x83,uint8_t(1));
 m.put(Memory::factory+0x10,14u);assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.primary==0);m.put(Memory::factory+0x10,13u);m.put(Memory::cells+(25*512+30)*4,0u);assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.primary==1&&plan.rallies==0);m.rally();m.put(Memory::type+0xEB8,-1);assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.rallies==0);m.put(Memory::type+0x16AC,uint8_t(1));assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.rallies==1);m.put(Memory::type+0x16AC,uint8_t(0));m.put(Memory::type+0xEB8,0x28);
 m.put(0x8192B8,1);assert(engine.capture(read,m.view,{object},640,600,plan));assert(nativefeedback::paintInteractions(plan.layers[2],{},image,bounds)&&bounds.w==20);
 const uint32_t ally=0x70000;const uint8_t rgb[]{0,255,0};m.put(ally+0x56F9,rgb);m.put(ally+0x30,2);m.put(m.view.house+0x5788,4u);m.put(Memory::factory+0x21C,ally);object.owner=ally;assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.primary==1&&plan.rallies==0);m.put(m.view.house+0x5788,0u);assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.primary==0);object=m.building();
 std::vector<std::array<int,2>>pixels;localui::patternLine({0,0},{10,0},0,[&](int x,int y){pixels.push_back({x,y});});assert(pixels.size()==8&&pixels[4][0]==4&&pixels[5][0]==8&&pixels.back()[0]==10);pixels.clear();localui::patternLine({3,3},{0,0},0,[&](int x,int y){pixels.push_back({x,y});});assert(pixels.size()==3&&pixels.front()[0]==0&&pixels.back()[0]==2);std::array<int,2>a{-10,5},b{10,5};assert(localui::clipLine(a,b,8,8)&&a[0]==0&&b[0]==7);a={-10,-1};b={10,-1};assert(!localui::clipLine(a,b,8,8));
 localui::Plan rally1,rally2;localui::Painter p1(rally1,640,600,640,600),p2(rally2,640,600,640,600);p1.rally({20,20},{120,70},0xFFFF0000,50);p2.rally({20,20},{120,70},0xFFFF0000,51);assert(rally1.layers[0]!=rally2.layers[0]);
 m.fail=Memory::label;assert(!engine.capture(read,m.view,{object},640,600,plan));for(const auto&lane:plan.layers)assert(lane.empty());m.fail=0;
 // New local-only tails: held Campaign timer, power text, group/rank and queued nodes.
 m.put(Memory::super+0x30,-1);m.put(Memory::super+0x24,-1);m.put(Memory::super+0x70,uint8_t(1));m.put(Memory::swType+0xB0,600);assert(engine.capture(read,m.view,{object},640,600,plan)&&plan.timers==2);m.put(Memory::super+0x30,10);m.put(Memory::super+0x70,uint8_t(0));
 constexpr uint32_t unit=0x700000,unitType=0x710000,shape=0x800000,converter=0x810000,palette=0x820000;nativefeedback::Object infantry{unit,71,unitType,m.view.house,{0,0,8,8},1};m.put(unit,0x7EB058u);m.put(unit+0x10,71u);m.put(unit+0x6C,100);m.put(unit+0x74,uint8_t(1));m.put(unit+0x90,uint8_t(1));m.put(unit+0x83,uint8_t(1));m.put(unit+0x6C0,unitType);m.put(unit+0x21C,m.view.house);m.put(unit+0x9C,nativefeedback::Point3{7680,6400,0});m.put(unit+0x214,9);m.put(unit+0x150,2.f);
 m.put(0xAC147C,shape);m.put(0x87F6C8,converter);m.put(converter,0x7E5358u);m.put(converter+4,2u);m.put(converter+0x16C,1);m.put(converter+0x174,palette);m.put(palette+2,uint16_t(0xF800));const uint16_t head[]{0,2,2,20};m.put(shape,head);for(int i=0;i<20;++i){m.put(shape+8+i*24+4,uint16_t(2));m.put(shape+8+i*24+6,uint16_t(2));m.put(shape+8+i*24+20,uint32_t(488+i*4));const uint8_t pixels[]{1,1,1,1};m.put(shape+488+i*4,pixels);}assert(engine.capture(read,m.view,{object,infantry},640,600,plan));bool rank=false;for(auto q:plan.layers[2])rank|=q.color==0xFFFF0000;assert(rank);
 m.vector(unit+0x588,0x720000,{Memory::cell});assert(engine.capture(read,m.view,{infantry},640,600,plan)&&!plan.layers[3].empty());m.put(unit+0x598,0);m.put(unit+0x214,-1);m.put(unit+0x150,0.f);
 // Sloped cells use the native floor coefficients; object destinations are resolved through their real identity.
 m.put(Memory::cell+0x11C,uint8_t(1));m.put(0x89E740,1.);const double terms[]{.25,0,0,104,0};m.put(0x81C900,terms);nativefeedback::Point3 target;assert(ground::target(read,Memory::cell,target)&&target.z==240);m.put(Memory::factory+0x218,unit);m.put(Memory::cells+(25*512+30)*4,Memory::cell);m.put(unit+0x9C,nativefeedback::Point3{30*256+128,25*256+128,0});assert(ground::target(read,unit,target)&&target.z==240);m.put(Memory::factory+0x218,Memory::cell);m.put(Memory::cell+0x11C,uint8_t(0));
 // The font discovery read can block without blocking the presentation caller.
 Reader::memory=&m;Reader::block=true;std::atomic<uint32_t>now{1000};localui::Task<Request,Reader>task([&]{return now.load();});Request request;request.view=m.view;auto started=std::chrono::steady_clock::now();task.submit(request,{object});assert(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(100));
 {std::unique_lock<std::mutex>lock(Reader::mutex);assert(Reader::cv.wait_for(lock,std::chrono::seconds(3),[]{return Reader::entered;}));}task.invalidate();++request.generation;task.submit(request,{object});{std::lock_guard<std::mutex>lock(Reader::mutex);Reader::block=false;Reader::cv.notify_all();}
 until([&]{return bool(task.visible(request,now));});auto first=task.visible(request,now);assert(first&&first->messages==1&&first->timers==3&&first->primary==1&&first->rallies==1);for(const auto&im:first->images)assert(im.pixels);auto changed=request;++changed.context.token;assert(!task.visible(changed,now));changed=request;++changed.pid;assert(!task.visible(changed,now));changed=request;++changed.match;assert(!task.visible(changed,now));changed=request;++changed.view.cameraX;assert(!task.visible(changed,now));assert(!task.visible(request,now+151));
 ++request.generation;task.submit(request,{object});until([&]{return bool(task.visible(request,now));});auto second=task.visible(request,now);for(size_t i=0;i<4;++i)assert(first->images[i].pixels==second->images[i].pixels);task.invalidate();assert(!task.get());task.shutdown();
 std::cout<<"PASS independent native-font masks, Chinese/CSF, spaces/missing glyphs, cache invalidation, message/edit/mission/SW/blackout UI, primary anchors/allies, rally phase/clip, four tight layers, Campaign hold, group/rank/queue UI, slope/object rally and asynchronous age/world rejection; no native-pixel certification\n";
}
