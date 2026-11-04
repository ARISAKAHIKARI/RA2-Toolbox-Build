"""Execute shipped host queue/catalogue/service; Win32 presentation is stubbed.
Geometry uses the same Layout as paint/hit-testing. Not a live game test.
"""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
source=(root/'assist/source/construction_host.h').read_text()
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
a=source.index('static external::ReadSession paintReader;');b=source.index('static void service()',a)
source=source[:a]+'static int proc=0;\nstatic void resetPaint(){paintReady=false;} static void requestPaint(){} static void pumpPaint(){}\n'+source[b:]
prefix=r'''
#include "native_hotkeys.h"
#include "construction_catalog.h"
#include "native_cameos.h"
#include "command_delivery.h"
#include <cassert>
#include <iostream>
#include <string>
#include <filesystem>
using namespace assist;
using DWORD=uint32_t;using HWND=void*;using HFONT=void*;using LRESULT=long;
struct RECT{long left=0,top=0,right=0,bottom=0;};struct POINT{long x=0,y=0;};
namespace gameinput{struct PointerPress {uint32_t tick=0,serial=0,match=0;int operation=0;POINT point{};int width=0,height=0;uint8_t keys[256]{};};}
struct WNDCLASSW{void*hInstance;int lpfnWndProc;const wchar_t*lpszClassName;void*hCursor;};
constexpr int VK_RBUTTON=2,VK_SHIFT=16,SW_HIDE=0,FALSE=0,FW_NORMAL=0,DEFAULT_CHARSET=0,OUT_DEFAULT_PRECIS=0,CLIP_DEFAULT_PRECIS=0,CLEARTYPE_QUALITY=0,DEFAULT_PITCH=0,IDC_HAND=0,WS_EX_TOOLWINDOW=0,WS_EX_NOACTIVATE=0,WS_EX_TOPMOST=0,WS_EX_LAYERED=0x80000,LWA_ALPHA=2,WS_POPUP=0,SWP_NOACTIVATE=1,SWP_SHOWWINDOW=2,SWP_NOOWNERZORDER=4,SWP_NOCOPYBITS=8,SWP_NOZORDER=16;
static void*HWND_TOPMOST=nullptr;static HWND game=(void*)2;static uint32_t now=1000,pid=1,match=1;static int clientWidth=3840,clientHeight=2160;
static bool enabled=true,focused=true,fresh=true,queueAccept=true;static std::wstring root=L"/tmp",message;
static int constructionScalePercent=100,soundRequests=0,panelAlpha=0;
static void requestReservationSound(){++soundRequests;}
static construction::InputPolicy reservationPolicy;static construction::InputClick inputClick;static bool inputPending=false;
static void publishReservationPolicy(const construction::InputPolicy&p){reservationPolicy=p;}
static bool takeReservationClick(construction::InputClick&out){if(!inputPending)return false;inputPending=false;out=inputClick;reservationPolicy={};return true;}
static bool SetLayeredWindowAttributes(HWND,int,int alpha,int flags){assert(flags==LWA_ALPHA);panelAlpha=alpha;return true;}
static struct{bool constructionQueue=true;}extra;
static struct{int typeCount=3,chat=0,renderStatus=1;struct{int kind=7,index=0,naval=0;char id[24]{};wchar_t name[40]{};}types[8];}snap;
static delivery::Queue commandQueue;
struct Reader{std::vector<uint8_t>bytes=std::vector<uint8_t>(0x1000000);uint32_t fail=0;
 bool readFresh(uint32_t p,void*out,size_t n){if(p<0x10000||p+n>bytes.size()||(fail&&p<=fail&&p+n>fail))return false;memcpy(out,bytes.data()+p,n);return true;}
 bool imagePath(wchar_t*,DWORD*){return false;}
 template<class T>void put(uint32_t p,T v){memcpy(bytes.data()+p,&v,sizeof v);}
}externalReader;
namespace assist { namespace construction {struct PanelBuffer{void clear(){}};}}
namespace um{struct IconCache{void open(const wchar_t*,const std::filesystem::path&){}};inline IconCache& sharedIconCache(){static IconCache c;return c;}}
static bool displayHold=false;
static bool constructionDisplayVisible(){return focused&&(snap.renderStatus==1||displayHold)&&!snap.chat;}
static bool constructionDisplayFresh(){return constructionDisplayVisible()&&snap.renderStatus==1&&!displayHold;}
static DWORD GetTickCount(){return now;}static bool foregroundGame(){return focused;}static bool commandFresh(uint32_t){return fresh;}static uint32_t commandMatch(){return match;}
static bool queueCommand(int c,int a,int b,uint32_t m,int retries=0,uint32_t delay=0,uint32_t token=0){return queueAccept&&commandQueue.add({c,a,b,pid,m,now,retries,now+delay,token});}
static void notify(const std::wstring&s){message=s;}static void critical(const std::wstring&){}
static bool IsWindowVisible(HWND){return true;}static bool GetWindowRect(HWND,RECT*){return false;}
static void ShowWindow(HWND,int){}static void InvalidateRect(HWND,void*,int){}static bool GetClientRect(HWND,RECT*r){r->right=clientWidth;r->bottom=clientHeight;return true;}static bool ClientToScreen(HWND,POINT*){return true;}
static void*GetModuleHandleW(void*){return nullptr;}static void*LoadCursorW(void*,int){return nullptr;}static void RegisterClassW(WNDCLASSW*){}
template<class...T>static HWND CreateWindowExW(T...){return (void*)3;}template<class...T>static HFONT CreateFontW(T...){return (void*)4;}template<class...T>static void SetWindowPos(T...){}
static void DeleteObject(void*){}static void DestroyWindow(void*){}
namespace overlaystack {enum class Layer { Construction };template<class...T>void place(T...) {}}
static struct {void shutdown(){}}paintWorker;
'''
prefix+=r'''
namespace nativekeyhost{static nativekeys::Bindings bindings;static uint8_t keys[256]{};static const nativekeys::Bindings&current(){return bindings;}static void down(uint8_t(&out)[256]){memcpy(out,keys,256);}}
'''
suffix=r'''
static constexpr uint32_t H=0x500000,C=0x510000,R=0x520000,T=0x600000,P=0x700000,F=0x710000;
static void w(uint32_t p,int v){externalReader.put(p,v);}static void byte(uint32_t p,uint8_t v){externalReader.put(p,v);}
static void list(uint32_t p,std::initializer_list<int> v){static uint32_t items=0x900000;w(p+4,items);w(p+8,v.size());w(p+16,v.size());for(int i:v){w(items,i);items+=4;}items+=32;}
static void setup(){
 buildreserve::reset();commandQueue.clear();soundRequests=0;externalReader.bytes.assign(0x1000000,0);externalReader.fail=0;extra.constructionQueue=true;focused=fresh=enabled=queueAccept=true;++match;++now;
 w(0xA8ED84,10);w(0xA83D4C,H);w(H+0x34,C);w(C+0xB4,0);w(C+0xB8,0);w(H+0x1D4,10);w(0x8871E0,R);
 list(0xA83C68,{T,T+0x2000,T+0x4000});list(R+0x358,{0});for(int g=1;g<6;++g)list(R+0x358+g*28,{});
 for(int i=0;i<3;++i){auto t=T+i*0x2000;snap.types[i].index=i;w(t+0x634,1);w(t+0x6CC,1);w(t+0xDA0,-1);w(t+0xDA4,-1);w(t+0xDF8,i);list(t+0x638,i==1?std::initializer_list<int>{-1}:i==2?std::initializer_list<int>{1}:std::initializer_list<int>{});list(t+0x654,{});}
 w(P+0x10,100);w(P+0x21C,H);w(P+0x6C,100);byte(P+0x74,1);byte(P+0x81,1);w(P+0x520,T);w(H+0x53BC,F);w(F+0x6C,H);w(F+0x58,P);
 w(0x87F7E8+0x539C,0);w(0x8A00A4,2560);w(0x8A00A8,1440);w(0x87F7E8+0x1544+84,1);w(0x87F7E8+0x1544+88,0);w(0x87F7E8+0x1544+92,7);
 now+=300;buildreserve::service();
}
static int choice(int index){for(int i=0;i<int(buildreserve::choices.size());++i)if(buildreserve::choices[i].index==index)return i;return -1;}
static void tick(int ms=110,bool advance=true){now+=ms;if(advance)w(0xA8ED84,buildreserve::get<int>(0xA8ED84)+1);buildreserve::service();}
static void place(){byte(P+0x81,0);byte(P+0x90,1);w(F+0x58,0);w(0x87F7E8+0x1544+84,3);for(int i=0;i<3;++i){w(0x87F7E8+0x1544+88+i*52,i);w(0x87F7E8+0x1544+92+i*52,7);}tick();assert(commandQueue.size==0);tick();}
int main(){
 using namespace buildreserve;
 setup();w(F+0x24,54);tick();assert(choice(0)>=0);reserve(choice(0));assert(books[0].items==std::vector<int>({0})&&commandQueue.size==0);
 setup();nativekeyhost::bindings.keys[nativekeys::Infantry][0]='I';nativekeyhost::keys['I']=1;tick();assert(activeTab==2&&get<int>(0x87F7E8+0x539C)==0);nativekeyhost::keys['I']=0;tick();assert(activeTab==2);snap.chat=1;nativekeyhost::keys['I']=1;tick();snap.chat=0;nativekeyhost::keys['I']=0;nativekeyhost::bindings={};

 setup();assert(choice(0)<0&&choice(1)>=0&&choice(2)<0);assert(choices[choice(1)].future);assert(layout.width()==216&&layout.x(60)==90&&layout.y(48)==72);
 constructionScalePercent=150;tick();assert(layout.width()==324&&layout.x(60)==135);constructionScalePercent=100;tick();
 reserve(choice(1));assert(choice(2)>=0);reserve(choice(2));reserve(choice(1));reserveChoice({0,0,0,T,false},true);reserve(choice(0));assert(books[0].items==std::vector<int>({1,2,1,0,0}));reserve(choice(0));assert(books[0].items.size()==5);
 removeTop();assert(books[0].items==std::vector<int>({1,2,1,0}));removeItem(0,1);assert(books[0].items==std::vector<int>({1,1,0}));removeTop(true);assert(books[0].items.empty());
 reserve(choice(1));w(P+0x6C,0);byte(P+0x74,0);for(int i=0;i<20;++i)tick();assert(books[0].items==std::vector<int>({1})&&commandQueue.size==0);w(F+0x58,0);tick();tick(300);assert(books[0].items.empty());
 setup();reserve(choice(1));w(F+0x58,0);tick();assert(!books[0].items.empty());tick(300);assert(books[0].items.empty());
 setup();reserve(choice(1));reserve(choice(2));place();assert(books[0].stage==1&&commandQueue.size==1);auto token=books[0].token;result(1,1,100,token+1);assert(books[0].stage==1);result(1,1,100,token);assert(books[0].stage==2);commandQueue.clear();
 uint32_t p2=P+0x3000;w(p2+0x10,101);w(p2+0x21C,H);w(p2+0x6C,100);byte(p2+0x74,1);byte(p2+0x81,1);w(p2+0x520,T+0x2000);w(F+0x58,p2);tick();assert(books[0].items==std::vector<int>({2})&&books[0].id==101&&books[0].stage==0);result(1,1,100,token);assert(books[0].stage==0);
 w(p2+0x6C,0);w(F+0x58,0);tick();tick(300);assert(books[0].items.empty());
 setup();reserve(choice(1));place();assert(commandQueue.size==1);extra.constructionQueue=false;tick();assert(commandQueue.size==0&&books[0].items.empty());
 setup();reserve(choice(1));place();++match;tick();assert(commandQueue.size==0&&books[0].items.empty());
 setup();reserve(choice(1));place();token=books[0].token;result(-9,0,100,token);commandQueue.clear();tick(1100);assert(books[0].stage==1&&books[0].attempts==2);result(-6,0,100,token);commandQueue.clear();tick(300);result(-6,0,100,token);assert(books[0].items.empty());
 setup();reserve(choice(1));place();token=books[0].token;
 for(int i=0;i<12;++i){result(-9,0,100,token);commandQueue.clear();tick(1100);assert(books[0].items==std::vector<int>({1})&&geometryReady&&choice(1)>=0);}
 setup();reserve(choice(1));externalReader.fail=P+0x74;for(int i=0;i<10;++i)tick();assert(books[0].items==std::vector<int>({1})&&commandQueue.size==0);externalReader.fail=0;
 setup();focused=false;reserve(choice(1));assert(books[0].items.empty());focused=true;queueAccept=false;reserve(choice(1));place();assert(!books[0].stage&&commandQueue.size==0);
 setup();externalReader.fail=T+0x2000+0x638+16;refreshChoices();assert(choice(1)<0&&choice(0)<0);externalReader.fail=0;
 // Native -1 sentinels, spawned country parent and stolen tech offsets.
 setup();w(T+0x2000+0xDA4,1);refreshChoices();assert(choice(1)<0);w(T+0x2000+0xDA4,-1);w(C+0xB8,4);refreshChoices();assert(choice(1)>=0);
 byte(T+0x2000+0xD9B,1);refreshChoices();assert(choice(1)<0);byte(H+0x2BC,1);refreshChoices();assert(choice(1)>=0);
 memcpy(externalReader.bytes.data()+C+0x98,"Americans",10);memcpy(externalReader.bytes.data()+C+0x24,"Americans",10);list(0xA83C98,{C});refreshChoices();assert(choice(1)>=0);
 construction::Layout l;for(int pct:{50,75,100,125,150,200,250})for(auto sizes:std::vector<std::vector<int>>{{2560,1440,3840,2160},{2560,1440,2560,1440},{1920,1080,3840,2160},{1280,720,1920,1080},{1920,1080,2560,1440},{640,480,640,480}}){assert(l.set(sizes[0],sizes[1],sizes[2],sizes[3],pct));l.content(5,50);assert(l.left()+l.width()<=sizes[2]);assert(l.top()+l.height()<=sizes[3]);for(int i=0;i<5;++i)assert(l.queueAt(l.x(10),l.y(construction::StackTop+i*construction::StackStep+9))==i);for(int i=0;i<l.rows*2;++i)assert(l.choiceAt(l.x((i%2)*72+36),l.y(l.headerHeight()+(i/2)*66+33),0,50)==i);assert(l.choiceAt(l.width(),l.y(l.headerHeight()+4),0,50)==-1);}

 // Regression: every reservation category, not only unavailable future types.
 for(int tab=0;tab<2;++tab)for(bool future:{false,true}){
  setup();if(tab){w(H+0x53BC,0);w(H+0x53CC,F);w(0x87F7E8+0x539C,1);for(int i=0;i<3;++i)w(T+i*0x2000+0xE08,5);uint32_t strip=0x87F7E8+0x1544+3988;w(strip+84,1);w(strip+88,0);w(strip+92,7);tick();}
  if(future)reserve(choice(1));else reserveChoice({0,0,tab,T,false},true);
  assert(books[tab].items==std::vector<int>({future?1:0}));
  w(P+0x6C,0);byte(P+0x74,0);
  for(int progress:{0,1,27,53,54}){w(F+0x24,progress);for(int k=0;k<10;++k)tick();assert(books[tab].items==std::vector<int>({future?1:0})&&commandQueue.size==0);}
  w(F+0x58,0);tick();tick(300);assert(books[tab].items.empty());
 }
 std::cout<<"PASS: building/defense x native/future at progress 0/1/27/53/54 survive factory limbo; parent cancellation retained\n";
 // Actual native sidebar capture: real SelectClass positions, scroll mapping,
 // click down/up ownership, no duplicate first production or placement click.
 auto nativeSetup=[&](){setup();uint32_t strip=0x87F7E8+0x1544;
  byte(strip+0x1C,1);w(strip+0x38,0);w(0x886F94,0);w(0x886F9C,1440);w(0xB0B4F8,220);w(0xA8B230,0x530000);
  for(int i=0;i<60;++i){uint32_t g=0xB07E80+i*56;w(g+0xC,2400+i%2*64);w(g+0x10,221+i/2*50);w(g+0x14,60);w(g+0x18,48);}
  w(F+0x24,10);};
 bool shift=false;auto press=[&](int op,int x=2420,int y=240){gameinput::PointerPress e;e.tick=now;e.match=match;e.serial=42;e.operation=op;e.width=clientWidth;e.height=clientHeight;e.keys[VK_RBUTTON]=op==5;e.keys[VK_SHIFT]=shift;e.point={x*3/2,y*3/2};return nativePointer(e);};
 auto deliverRemove=[&](bool all=false){inputClick={now,match,H,P,100,0,0,2,uint32_t(all)};inputPending=true;consumeNativeClick();};
 auto deliver=[&](int index=0){inputClick={now,match,H,P,100,0,index,1,0};inputPending=true;consumeNativeClick();};
 nativeSetup();assert(reservationPolicy.slots==5);assert(press(7));assert(press(6));assert(books[0].items.empty()); // polling cannot duplicate Runtime input
 deliver();assert(books[0].items==std::vector<int>({0})&&soundRequests==1&&reservationPolicy.slots==4);
 auto badges=nativeBadges();assert(badges.size()==1&&badges[0].x==2400&&badges[0].y==253&&badges[0].positions==1);
 deliver();assert(books[0].items.size()==2&&soundRequests==2);press(5);press(4);assert(books[0].items.size()==2);deliverRemove();assert(books[0].items.size()==1);
 shift=true;press(5);press(4);shift=false;deliverRemove(true);assert(books[0].items.empty());
 nativeSetup();deliver();press(5);press(4,2440);assert(books[0].items.size()==1);
 nativeSetup();inputClick={now,match,H,P,100,0,0};inputPending=true;++match;consumeNativeClick();assert(books[0].items.empty()&&!soundRequests);
 nativeSetup();inputClick={now-301,match,H,P,100,0,0};inputPending=true;consumeNativeClick();assert(books[0].items.empty());
 nativeSetup();inputClick={now,match,H,P,999,0,0};inputPending=true;consumeNativeClick();assert(books[0].items.empty());
 nativeSetup();focused=false;deliver();assert(books[0].items.empty());focused=true;
 nativeSetup();w(0x87F7E8+0x1544+84,3);w(0x87F7E8+0x1544+88+2*52,2);w(0x87F7E8+0x1544+92+2*52,7);deliver(2);assert(books[0].items==std::vector<int>({2}));
 nativeSetup();for(int i=0;i<5;++i)deliver();assert(reservationPolicy.slots==0&&soundRequests==5);deliver();assert(books[0].items.size()==5&&soundRequests==5);
 hide();assert(!reservationPolicy.slots);
 std::cout<<"PASS: Runtime input consumer exactly once, no polling duplicate, native right/Shift cancel, match/age/parent/focus/capacity authorization\n";
 // Same game frame: wall-clock ticks cannot release next Produce.
 setup();reserve(choice(1));byte(P+0x81,0);byte(P+0x90,1);w(F+0x58,0);w(0x87F7E8+0x1544+84,2);w(0x87F7E8+0x1544+88+52,1);w(0x87F7E8+0x1544+92+52,7);
 tick(110,false);for(int i=0;i<20;++i)tick(110,false);assert(commandQueue.size==0);tick();assert(commandQueue.size==1);
 // Old placed building can be sold/freed before observing the accepted new
 // product. Adopt new parent first, retain remaining queue and visible panel.
 setup();reserve(choice(1));reserve(choice(2));place();token=books[0].token;result(1,1,100,token);commandQueue.clear();
 w(P+0x10,999);w(p2+0x10,101);w(p2+0x21C,H);w(p2+0x520,T+0x2000);byte(p2+0x81,1);w(F+0x58,p2);tick();assert(books[0].items==std::vector<int>({2})&&books[0].id==101&&geometryReady);
 // A sole reservation becoming current production must not hide the panel.
 setup();reserveChoice({0,0,0,T,false},true);place();token=books[0].token;result(1,1,100,token);commandQueue.clear();
 w(p2+0x10,101);w(p2+0x21C,H);w(p2+0x520,T);byte(p2+0x81,1);w(F+0x58,p2);tick();assert(books[0].items.empty()&&geometryReady&&current[0]==p2);
 std::cout<<"PASS: overlapping stack, native right cancel, Shift clear, paused frame gate, old-parent reuse, panel stays during final reserved production\n";
 std::cout<<"PASS: native sidebar badges and Runtime-authorized input delivery\n";


 // Content fitting: no padded rows, shrinking, scrolling and hit regions use
 // the actual queue/candidates at every supported transform.
 for(int pct:{50,75,100,125,150,200,250})for(auto sizes:std::vector<std::vector<int>>{{2560,1440,3840,2160},{2560,1440,2560,1440},{1920,1080,3840,2160},{1280,720,1920,1080},{1920,1080,2560,1440},{640,480,640,480}})
 for(int q=0;q<=5;++q)for(int candidates:{0,1,2,3,5,12,13,50}){
  assert(l.set(sizes[0],sizes[1],sizes[2],sizes[3],pct));l.content(q,candidates);
  assert(l.top()+l.height()<=sizes[3]&&l.left()+l.width()<=sizes[2]);
  assert(l.rows<=6&&l.rows<=(candidates+1)/2&&((l.rows==0)==(candidates==0)));
  assert(l.height()==l.y(l.headerHeight()+l.rows*66));assert(l.headerHeight()==l.stackBottom()+construction::SectionGap);assert(l.choiceAt(l.x(36),l.y(l.headerHeight()-5),0,candidates)==-1);
  for(int i=0;i<q;++i)assert(l.queueAt(l.x(10),l.y(24+i*24+9),q)==i);
  for(int i=0;i<std::min(candidates,l.rows*2);++i)assert(l.choiceAt(l.x(i%2*72+36),l.y(l.headerHeight()+i/2*66+33),0,candidates)==i);
  assert(l.choiceAt(l.x(36),l.height(),0,candidates)==-1);
 }
 setup();reserve(choice(1));int addedSound=soundRequests;removeTop();assert(soundRequests==addedSound+1);removeTop();assert(soundRequests==addedSound+1);
 reserve(choice(1));reserve(choice(1));addedSound=soundRequests;removeTop(true);assert(soundRequests==addedSound+1); // clear once, not once per item
 reserve(choice(1));place();commandQueue.clear();addedSound=soundRequests;removeItem(0,0);assert(soundRequests==addedSound); // submitted head cannot be removed
 setup();assert(panelAlpha==construction::PanelOpacity&&layout.rows==1&&layout.queueCount==0&&layout.height()==layout.y(132));
 int shortHeight=layout.height();reserve(choice(1));assert(soundRequests==1&&layout.queueCount==1&&layout.height()>shortHeight);
 reserveChoice({0,0,0,T,false},true);assert(soundRequests==1); // original sidebar owns its sound
 focused=false;reserve(choice(1));assert(soundRequests==1);focused=true;
 while(books[0].items.size()<5)reserve(choice(1));int heard=soundRequests;reserve(choice(1));assert(soundRequests==heard); // rejected click stays quiet
 removeTop(true);assert(layout.queueCount==0&&layout.height()==shortHeight);
 std::cout<<"PASS: 2016 content/scale/viewport combinations, fitted growth/shrink, 236/255 alpha, successful add/cancel feedback, clear once and no sound on empty/locked cancel\n";
 // A live read-only panel must survive a suspended command lane. Native
 // input authorization closes and no Produce is issued until commands resume.
 setup();reserve(choice(1));fresh=false;tick();assert(geometryReady&&books[0].items==std::vector<int>({1})&&reservationPolicy.slots==0);
 reserve(choice(1));assert(books[0].items.size()==1);place();assert(commandQueue.size==0&&geometryReady);
 fresh=true;tick();assert(commandQueue.size==0);tick();assert(commandQueue.size==1);
 setup();displayHold=true;snap.renderStatus=0;tick(1);assert(geometryReady&&reservationPolicy.slots==0);displayHold=false;snap.renderStatus=1;tick();assert(geometryReady&&reservationPolicy.slots==5);
 setup();snap.chat=1;tick();assert(!geometryReady&&reservationPolicy.slots==0);snap.chat=0;tick();assert(reservationPolicy.slots==5);
 snap.renderStatus=0;tick();assert(!geometryReady&&reservationPolicy.slots==0);snap.renderStatus=1;tick();assert(geometryReady);
 focused=false;tick();assert(!geometryReady);focused=true;tick();assert(geometryReady);
 std::cout<<"PASS: read-only panel survives command suspension; closed input/no Produce; resumes once; chat/menu/focus hide consistently with main gate\n";

 // A queued barracks predicts infantry; no infantry factory exists yet.
 setup();reserve(choice(1));w(T+0x2000+0xEB8,16);uint32_t I=T+0x6000;
 list(0xA8E348,{I});w(I+0x634,1);w(I+0x6CC,1);w(I+0xDA0,-1);w(I+0xDA4,-1);list(I+0x638,{1});list(I+0x654,{});
 snap.typeCount=4;snap.types[3].kind=16;snap.types[3].index=0;
 selectedTab=activeTab=2;refreshChoices();assert(choice(encode(16,0))>=0);reserve(choice(encode(16,0)));
 assert(books[2].items.size()==1&&!books[2].source);tick();assert(books[2].items.size()==1&&commandQueue.size==0);
 removeItem(0,0,true);tick();tick(600);assert(books[2].items.empty()); // dependency canceled
 // Cross-tab defense preview while only its barracks prerequisite is queued.
 snap.typeCount=3;setup();reserve(choice(1));w(T+0x4000+0xE08,5);
 selectedTab=activeTab=1;refreshChoices();assert(choice(2)>=0);reserve(choice(2));assert(!books[1].source);
 removeItem(0,0,true);tick();tick(600);assert(books[1].items.empty());
 // Unit production waits for native cameo; consumes exactly once at ACK.
 snap.typeCount=3;setup();reserve(choice(1));w(T+0x2000+0xEB8,16);
 list(0xA8E348,{I});w(I+0x634,1);w(I+0x6CC,1);w(I+0xDA0,-1);w(I+0xDA4,-1);list(I+0x638,{1});list(I+0x654,{});
 snap.typeCount=4;snap.types[3].kind=16;snap.types[3].index=0;
 selectedTab=activeTab=2;refreshChoices();reserve(choice(encode(16,0)));tick();assert(commandQueue.size==0);
 uint32_t infantryStrip=0x87F7E8+0x1544+2*3988;w(infantryStrip+84,1);w(infantryStrip+88,0);w(infantryStrip+92,16);
 tick();assert(commandQueue.size==1&&commandQueue.slots[0].command==ProduceDeferred);
 auto deferredToken=books[2].token;result(1,1,0,deferredToken);assert(books[2].items.empty());result(1,1,0,deferredToken);assert(books[2].items.empty());
 std::cout<<"PASS: missing-barracks infantry, cross-tab defense, canceled prerequisite cascade, native-cameo wait and exactly-once unit ACK\n";
 // The original unit sidebar works even when no building is producing. Its
 // first native cameo must open an empty reservation panel and allow a click.
 setup();snap.typeCount=4;snap.types[3].kind=16;snap.types[3].index=0;
 list(0xA8E348,{int(I)});w(I+0x634,1);w(I+0x6CC,1);w(I+0xDA0,-1);w(I+0xDA4,-1);list(I+0x638,{});list(I+0x654,{});
 w(F+0x58,0);selectedTab=activeTab=2;w(infantryStrip+84,1);w(infantryStrip+88,0);w(infantryStrip+92,16);
 tick();assert(geometryReady&&choice(encode(16,0))>=0&&books[2].items.empty());reserve(choice(encode(16,0)));assert(books[2].items.size()==1);
 // All 30 reservations drain into the same native FactoryClass queue while
 // the previous unit is still active; the 31st reservation is rejected.
 setup();snap.typeCount=4;snap.types[3].kind=16;snap.types[3].index=0;
 list(0xA8E348,{int(I)});w(I+0x634,1);w(I+0x6CC,1);w(I+0xDA0,-1);w(I+0xDA4,-1);list(I+0x638,{});list(I+0x654,{});
 w(H+0x53B0,F);w(F+0x6C,H);w(F+0x58,0);w(F+0x50,0);
 w(infantryStrip+84,1);w(infantryStrip+88,0);w(infantryStrip+92,16);selectedTab=activeTab=2;refreshChoices();
 assert(choice(encode(16,0))>=0);for(int i=0;i<30;++i)reserve(choice(encode(16,0)));
 assert(books[2].items.size()==30&&books[2].visibleStart()==25&&layout.queueCount==5);
 int sounds=soundRequests;reserve(choice(encode(16,0)));assert(books[2].items.size()==30&&sounds==soundRequests);
 for(int accepted=0;accepted<30;++accepted){tick(accepted?20:110);assert(commandQueue.size==1&&books[2].stage==1&&commandQueue.slots[0].arg==3);
  uint32_t token=books[2].token;commandQueue.clear();result(1,1,0,token);result(1,1,0,token);
  w(F+0x58,P);w(F+0x50,accepted); // one active, remainder in native QueuedObjects
 }
 assert(books[2].items.empty()&&commandQueue.size==0&&unitBudgets[0].pending==1);
 uint32_t replacement=F+0x400;w(H+0x53B0,replacement);w(replacement+0x6C,H);w(replacement+0x58,0);w(replacement+0x50,29);
 reserve(choice(encode(16,0)));tick();assert(commandQueue.size==0&&books[2].items.size()==1); // unobserved ACK survives factory replacement
 w(replacement+0x50,28);tick();assert(commandQueue.size==1);commandQueue.clear();result(-13,0,0,books[2].token);
 // First native Produce can allocate a new FactoryClass. Its first product
 // reconciles the ACK shadow instead of consuming two slots in the budget.
 setup();snap.typeCount=4;snap.types[3].kind=16;snap.types[3].index=0;
 list(0xA8E348,{int(I)});w(I+0x634,1);w(I+0x6CC,1);w(I+0xDA0,-1);w(I+0xDA4,-1);list(I+0x638,{});list(I+0x654,{});
 w(infantryStrip+84,1);w(infantryStrip+88,0);w(infantryStrip+92,16);selectedTab=activeTab=2;refreshChoices();
 reserve(choice(encode(16,0)));reserve(choice(encode(16,0)));tick();assert(commandQueue.size==1);
 deferredToken=books[2].token;commandQueue.clear();result(1,1,0,deferredToken);assert(unitBudgets[0].pending==1);
 w(H+0x53B0,F);w(F+0x6C,H);w(F+0x58,P);w(F+0x50,0);tick(20);assert(commandQueue.size==1&&unitBudgets[0].pending==0);
 commandQueue.clear();result(-13,0,0,books[2].token);
 // At 29 native waiting plus one active, keep the tail. ACKed events count
 // against capacity until FactoryClass's vector reflects them.
 setup();snap.typeCount=4;snap.types[3].kind=16;snap.types[3].index=0;
 list(0xA8E348,{int(I)});w(I+0x634,1);w(I+0x6CC,1);w(I+0xDA0,-1);w(I+0xDA4,-1);list(I+0x638,{});list(I+0x654,{});
 w(H+0x53B0,F);w(F+0x6C,H);w(F+0x58,P);w(F+0x50,29);
 w(infantryStrip+84,1);w(infantryStrip+88,0);w(infantryStrip+92,16);selectedTab=activeTab=2;refreshChoices();
 reserve(choice(encode(16,0)));reserve(choice(encode(16,0)));tick();assert(commandQueue.size==0&&books[2].items.size()==2);
 w(F+0x50,28);tick();assert(commandQueue.size==1);deferredToken=books[2].token;commandQueue.clear();result(1,1,0,deferredToken);
 tick(20);assert(commandQueue.size==0&&books[2].items.size()==1);w(F+0x50,29);tick();assert(commandQueue.size==0);
 w(F+0x50,28);tick();assert(commandQueue.size==1);commandQueue.clear();result(-9,0,0,books[2].token);assert(books[2].items.size()==1);
 removeTop(true);assert(books[2].items.empty());
 // A ground vehicle, plane and ship have distinct factory slots. A busy
 // vehicle factory must not block an aircraft or ship in the same tab.
 setup();uint32_t V=T+0x8000,N=T+0xA000,A=T+0xC000;
 list(0xA83CE0,{int(V),int(N)});list(0xA8B218,{int(A)});byte(N+0xCCE,1);
 for(uint32_t p:{V,N,A}){w(p+0x634,1);w(p+0x6CC,1);w(p+0xDA0,-1);w(p+0xDA4,-1);list(p+0x638,{});list(p+0x654,{});}
 snap.typeCount=6;snap.types[3].kind=40;snap.types[3].index=0;snap.types[4].kind=3;snap.types[4].index=0;snap.types[5].kind=40;snap.types[5].index=1;snap.types[5].naval=1;
 uint32_t vehicleStrip=0x87F7E8+0x1544+3*3988;w(vehicleStrip+84,3);w(vehicleStrip+88,0);w(vehicleStrip+92,40);w(vehicleStrip+88+52,0);w(vehicleStrip+92+52,3);w(vehicleStrip+88+104,1);w(vehicleStrip+92+104,40);
 uint32_t groundF=F,airF=F+0x100,seaF=F+0x200;w(H+0x53B4,groundF);w(H+0x53AC,airF);w(H+0x53B8,seaF);
 for(uint32_t f:{groundF,airF,seaF}){w(f+0x6C,H);w(f+0x58,P);w(f+0x50,0);}
 selectedTab=activeTab=3;refreshChoices();for(int index:{encode(40,0),encode(3,0),encode(40,1)}){int c=choice(index);assert(c>=0);reserve(c);}
 assert(books[3].items.size()==3);for(int slot:{3,4,5}){tick();assert(commandQueue.size==1&&commandQueue.slots[0].arg==slot);
  uint32_t token=books[3].token;commandQueue.clear();result(1,1,0,token);assert(books[3].items.size()==size_t(5-slot));}
 std::cout<<"PASS: visible first unit cameo, 30-item burst/31st rejection, native full queue and ACK shadow, separate ground/air/naval factories\n";
 assert(!l.set(0,0,3840,2160));
 std::cout<<"PASS: actual host service FIFO5/duplicates/future unlock/cancel parent/right cancel/round/token ACK/retry/busy/menu; catalogue sentinels/parent/stolen flags/read failure; six resolution transforms and hit regions\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'host.cpp';exe=Path(tmp)/'host';cpp.write_text(prefix+source+suffix)
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'assist/source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0"))

