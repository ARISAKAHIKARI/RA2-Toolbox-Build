"""Execute shipped host queue/catalogue/service; Win32 presentation is stubbed.
Geometry uses the same Layout as paint/hit-testing. Not a live game test.
"""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
source=(root/'assist/source/construction_host.h').read_text()
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
a=source.index('static RECT rect(');b=source.index('static void service()',a)
source=source[:a]+'static int proc=0;\n'+source[b:]
prefix=r'''
#include "construction_catalog.h"
#include "command_delivery.h"
#include <cassert>
#include <iostream>
#include <string>
#include <filesystem>
using namespace assist;
using DWORD=uint32_t;using HWND=void*;using HFONT=void*;using LRESULT=long;
struct RECT{long left=0,top=0,right=0,bottom=0;};struct POINT{long x=0,y=0;};
struct WNDCLASSW{void*hInstance;int lpfnWndProc;const wchar_t*lpszClassName;void*hCursor;};
constexpr int SW_HIDE=0,FALSE=0,FW_NORMAL=0,DEFAULT_CHARSET=0,OUT_DEFAULT_PRECIS=0,CLIP_DEFAULT_PRECIS=0,CLEARTYPE_QUALITY=0,DEFAULT_PITCH=0,IDC_HAND=0,WS_EX_TOOLWINDOW=0,WS_EX_NOACTIVATE=0,WS_EX_TOPMOST=0,WS_POPUP=0,SWP_NOACTIVATE=0,SWP_SHOWWINDOW=0;
static void*HWND_TOPMOST=nullptr;static HWND game=(void*)2;static uint32_t now=1000,pid=1,match=1;static int clientWidth=3840,clientHeight=2160;
static bool enabled=true,focused=true,fresh=true,queueAccept=true;static std::wstring root=L"/tmp",message;
static struct{bool constructionQueue=true;}extra;
static struct{int typeCount=3,chat=0,renderStatus=1;struct{int kind=7,index=0;char id[24]{};wchar_t name[40]{};}types[8];}snap;
static delivery::Queue commandQueue;
struct Reader{std::vector<uint8_t>bytes=std::vector<uint8_t>(0x1000000);uint32_t fail=0;
 bool readFresh(uint32_t p,void*out,size_t n){if(p<0x10000||p+n>bytes.size()||(fail&&p<=fail&&p+n>fail))return false;memcpy(out,bytes.data()+p,n);return true;}
 bool imagePath(wchar_t*,DWORD*){return false;}
 template<class T>void put(uint32_t p,T v){memcpy(bytes.data()+p,&v,sizeof v);}
}externalReader;
namespace um{struct IconCache{void open(const wchar_t*,const std::filesystem::path&){}};}
static DWORD GetTickCount(){return now;}static bool foregroundGame(){return focused;}static bool commandFresh(uint32_t){return fresh;}static uint32_t commandMatch(){return match;}
static bool queueCommand(int c,int a,int b,uint32_t m,int retries=0,uint32_t delay=0,uint32_t token=0){return queueAccept&&commandQueue.add({c,a,b,pid,m,now,retries,now+delay,token});}
static void notify(const std::wstring&s){message=s;}static void critical(const std::wstring&){}
static void ShowWindow(HWND,int){}static void InvalidateRect(HWND,void*,int){}static bool GetClientRect(HWND,RECT*r){r->right=clientWidth;r->bottom=clientHeight;return true;}static bool ClientToScreen(HWND,POINT*){return true;}
static void*GetModuleHandleW(void*){return nullptr;}static void*LoadCursorW(void*,int){return nullptr;}static void RegisterClassW(WNDCLASSW*){}
template<class...T>static HWND CreateWindowExW(T...){return (void*)3;}template<class...T>static HFONT CreateFontW(T...){return (void*)4;}template<class...T>static void SetWindowPos(T...){}
static void DeleteObject(void*){}static void DestroyWindow(void*){}
'''
suffix=r'''
static constexpr uint32_t H=0x500000,C=0x510000,R=0x520000,T=0x600000,P=0x700000,F=0x710000;
static void w(uint32_t p,int v){externalReader.put(p,v);}static void byte(uint32_t p,uint8_t v){externalReader.put(p,v);}
static void list(uint32_t p,std::initializer_list<int> v){static uint32_t items=0x900000;w(p+4,items);w(p+8,v.size());w(p+16,v.size());for(int i:v){w(items,i);items+=4;}items+=32;}
static void setup(){
 buildreserve::reset();commandQueue.clear();externalReader.bytes.assign(0x1000000,0);externalReader.fail=0;extra.constructionQueue=true;focused=fresh=enabled=queueAccept=true;++match;++now;
 w(0xA83D4C,H);w(H+0x34,C);w(C+0xB4,0);w(C+0xB8,0);w(H+0x1D4,10);w(0x8871E0,R);
 list(0xA83C68,{T,T+0x2000,T+0x4000});list(R+0x358,{0});for(int g=1;g<6;++g)list(R+0x358+g*28,{});
 for(int i=0;i<3;++i){auto t=T+i*0x2000;snap.types[i].index=i;w(t+0x634,1);w(t+0x6CC,1);w(t+0xDA0,-1);w(t+0xDA4,-1);w(t+0xDF8,i);list(t+0x638,i==1?std::initializer_list<int>{-1}:i==2?std::initializer_list<int>{1}:std::initializer_list<int>{});list(t+0x654,{});}
 w(P+0x10,100);w(P+0x21C,H);w(P+0x6C,100);byte(P+0x74,1);byte(P+0x81,1);w(P+0x520,T);w(H+0x53BC,F);w(F+0x6C,H);w(F+0x58,P);
 w(0x87F7E8+0x539C,0);w(0xA8EB84,2560);w(0xA8EB88,1440);w(0x87F7E8+0x1544+84,1);w(0x87F7E8+0x1544+88,0);w(0x87F7E8+0x1544+92,7);
 now+=300;buildreserve::service();
}
static int choice(int index){for(int i=0;i<int(buildreserve::choices.size());++i)if(buildreserve::choices[i].index==index)return i;return -1;}
static void tick(int ms=110){now+=ms;buildreserve::service();}
static void place(){byte(P+0x81,0);byte(P+0x90,1);w(F+0x58,0);w(0x87F7E8+0x1544+84,3);for(int i=0;i<3;++i){w(0x87F7E8+0x1544+88+i*52,i);w(0x87F7E8+0x1544+92+i*52,7);}tick();}
int main(){
 using namespace buildreserve;
 setup();assert(choice(0)>=0&&choice(1)>=0&&choice(2)<0);assert(choices[choice(1)].future);assert(layout.width()==216&&layout.x(60)==90&&layout.y(48)==72);
 reserve(choice(1));assert(choice(2)>=0);reserve(choice(2));reserve(choice(1));reserve(choice(0));reserve(choice(0));assert(books[0].items==std::vector<int>({1,2,1,0,0}));reserve(choice(0));assert(books[0].items.size()==5);
 removeFrom(3);assert(books[0].items.size()==3);removeFrom(1);assert(books[0].items==std::vector<int>({1}));removeFrom(0);assert(books[0].items.empty());
 reserve(choice(1));w(P+0x6C,0);tick();assert(books[0].items.empty()&&commandQueue.size==0);
 setup();reserve(choice(1));w(F+0x58,0);tick();assert(!books[0].items.empty());tick(300);assert(books[0].items.empty());
 setup();reserve(choice(1));reserve(choice(2));place();assert(books[0].stage==1&&commandQueue.size==1);auto token=books[0].token;result(1,1,100,token+1);assert(books[0].stage==1);result(1,1,100,token);assert(books[0].stage==2);commandQueue.clear();
 uint32_t p2=P+0x3000;w(p2+0x10,101);w(p2+0x21C,H);w(p2+0x6C,100);byte(p2+0x74,1);byte(p2+0x81,1);w(p2+0x520,T+0x2000);w(F+0x58,p2);tick();assert(books[0].items==std::vector<int>({2})&&books[0].id==101&&books[0].stage==0);result(1,1,100,token);assert(books[0].stage==0);
 w(p2+0x6C,0);tick();assert(books[0].items.empty());
 setup();reserve(choice(1));place();assert(commandQueue.size==1);extra.constructionQueue=false;tick();assert(commandQueue.size==0&&books[0].items.empty());
 setup();reserve(choice(1));place();++match;tick();assert(commandQueue.size==0&&books[0].items.empty());
 setup();reserve(choice(1));place();token=books[0].token;result(-9,0,100,token);commandQueue.clear();tick(300);assert(books[0].stage==1&&books[0].attempts==2);result(-6,0,100,token);commandQueue.clear();tick(300);result(-6,0,100,token);assert(books[0].items.empty());
 setup();focused=false;reserve(choice(1));assert(books[0].items.empty());focused=true;queueAccept=false;reserve(choice(1));place();assert(!books[0].stage&&commandQueue.size==0);
 setup();externalReader.fail=T+0x2000+0x638+16;refreshChoices();assert(choice(1)<0&&choice(0)>=0);externalReader.fail=0;
 // Native -1 sentinels, spawned country parent and stolen tech offsets.
 setup();w(T+0x2000+0xDA4,1);refreshChoices();assert(choice(1)<0);w(T+0x2000+0xDA4,-1);w(C+0xB8,4);refreshChoices();assert(choice(1)>=0);
 byte(T+0x2000+0xD9B,1);refreshChoices();assert(choice(1)<0);byte(H+0x2BC,1);refreshChoices();assert(choice(1)>=0);
 memcpy(externalReader.bytes.data()+C+0x98,"Americans",10);memcpy(externalReader.bytes.data()+C+0x24,"Americans",10);list(0xA83C98,{C});refreshChoices();assert(choice(1)>=0);
 construction::Layout l;for(auto sizes:std::vector<std::vector<int>>{{2560,1440,3840,2160},{2560,1440,2560,1440},{1920,1080,3840,2160},{1280,720,1920,1080},{1920,1080,2560,1440},{640,480,640,480}}){assert(l.set(sizes[0],sizes[1],sizes[2],sizes[3]));assert(l.left()+l.width()<=sizes[2]);assert(l.top()+l.height()<=sizes[3]);for(int i=0;i<5;++i)assert(l.queueAt(l.x(10),l.y(24+i*18+9))==i);for(int i=0;i<l.rows*2;++i)assert(l.choiceAt(l.x((i%2)*72+36),l.y(116+(i/2)*66+33),0,50)==i);assert(l.choiceAt(l.width(),l.y(120),0,50)==-1);}
 assert(!l.set(0,0,3840,2160));
 std::cout<<"PASS: actual host service FIFO5/duplicates/future unlock/cancel parent/right cancel/round/token ACK/retry/busy/menu; catalogue sentinels/parent/stolen flags/read failure; six resolution transforms and hit regions\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp=Path(tmp)/'host.cpp';exe=Path(tmp)/'host';cpp.write_text(prefix+source+suffix)
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'assist/source'),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0"))
