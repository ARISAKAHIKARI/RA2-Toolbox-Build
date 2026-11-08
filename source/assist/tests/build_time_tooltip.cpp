// The Python runner extracts the actual entry and unchanged bridge helpers.
// Win32 process memory/lease and the native call are explicit service seams.
#include <cstdint>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>
#include <utility>
#include "../source/build_time_types.h"
using namespace assist;
using DWORD=uint32_t;
static_assert(sizeof(wchar_t)==2,"exercise UTF-16 buffer units");
static std::vector<uint8_t> memory(0x2000000),pristine;
static unsigned checks=0,scenarios=0,nativeCalls=0,totalNativeCalls=0;
static const char* scenario="startup";
static void checked(bool ok,int line){++checks;if(!ok){std::fprintf(stderr,"FAIL %s at line %d\n",scenario,line);std::abort();}}
#define check(x) checked(bool(x),__LINE__)
template<class T>static void put(uint32_t at,T value){check(uint64_t(at)+sizeof value<=memory.size());std::memcpy(memory.data()+at,&value,sizeof value);}
static uint32_t readFailure=0,afterAddress=0;
static std::function<void()> afterRead,nativeMutation;
static bool read(uint32_t at,void*out,size_t n){
 if(at<0x10000||uint64_t(at)+n>memory.size()||(readFailure&&at<=readFailure&&uint64_t(at)+n>readFailure))return false;
 std::memcpy(out,memory.data()+at,n);
 if(afterRead&&at==afterAddress){auto action=std::move(afterRead);afterRead={};action();}
 return true;
}
template<class T>static T get(uint32_t at){T value{};check(read(at,&value,sizeof value));return value;}
#include "production-bridge-helpers.inc"
struct Shared {bool stop=false,buildTimeTooltip=true,foreground=true;uint32_t heartbeat=1000;int buildTimeTooltipStatus=0;} shared,*sh=&shared;
static uint32_t match=1,lastHouse=0x1100000,now=1000;
static bool fault=false,claimed=false;
struct Snapshot {int status=1;uint32_t match=1;}snap;
static DWORD sampleClock(Shared*){return now;}
namespace singlefps {static unsigned custom=0;static unsigned activeRate(){return custom;}}
extern "C" bool NativeTooltipBuildTime(bool wanted){claimed=wanted;return true;}
constexpr uint32_t House=0x1100000,Factory=0x1180000,Product=0x1181000,OtherProduct=0x1182000,Original=0x11A0000,Sidebar=0x87F7E8;
static const buildtime::TypeInfo* selected=nullptr;
static uint32_t selectedType=0,strip=0,cameo=0;
static int nativeRaw=900;
template<class R,class...Args>static R call(uint32_t address,uint32_t self,Args...){
 ++nativeCalls;++totalNativeCalls;check(address==0x6F47A0&&selected);
 // Only the test process uses a 64-bit stack. Recover the same stack page;
 // the shipping x86 entry passes this exact prefix directly in ECX.
 uintptr_t probe=uintptr_t(&address),pointer=(probe&~uintptr_t(UINT32_MAX))|self;
 check(pointer>probe-0x100000&&pointer<probe+0x100000);
 const auto*receiver=reinterpret_cast<const uint8_t*>(pointer);
 uint8_t expected[buildtime::ReceiverBytes]{};
 std::memcpy(expected,&selected->vtable,4);std::memcpy(expected+selected->typeOffset,&selectedType,4);std::memcpy(expected+0x21C,&House,4);
 check(!std::memcmp(receiver,expected,sizeof expected));
 if(nativeMutation){auto action=std::move(nativeMutation);nativeMutation={};action();}
 return R(nativeRaw);
}
#include "production-tooltip.inc"

static std::u16string originalText=u"战车\n$1000";
static void begin(const char*name){scenario=name;++scenarios;}
static void original(const std::u16string&text){for(size_t i=0;i<=text.size();++i)put(Original+uint32_t(i)*2,uint16_t(i<text.size()?text[i]:0));}
static uint32_t typeAt(unsigned i){return 0x1110000+i*0x2000;}
static void reset(int kind=7,int tab=0,int count=10,int row=0,int index=0){
 memory=pristine;readFailure=afterAddress=0;afterRead={};nativeMutation={};shared={};sh=&shared;snap={};match=1;lastHouse=House;fault=false;now=1000;
 buildTimeTooltipActive=true;buildTimeTooltipFailed=false;buildTimeTooltipFailedMatch=0;singlefps::custom=0;claimed=true;nativeCalls=0;nativeRaw=900;
 debugentries::applyPolicy(debugentries::Config{});
 selected=buildtime::typeInfo(kind);selectedType=0;
 for(unsigned i=0;i<4;++i){const auto&t=buildtime::Types[i];const uint32_t items=0x1190000+i*0x100;Vec v{0,items,2,0,1,1};put(t.array,v);put(items,typeAt(i));put(typeAt(i),t.typeVtable);if(&t==selected)selectedType=typeAt(i);}
 put(0xA8E9A0,uint8_t(1));put(0xA83D4C,House);put(0xA8ED84,100);put(0xA8B238,9);put(0xA8EB60,0);
 put(Sidebar+0x539C,tab);strip=Sidebar+0x1544+uint32_t(tab>=0&&tab<4?tab:0)*3988;
 put(strip+0x44,row);put(strip+0x54,count);cameo=strip+88+uint32_t(index)*52;put(cameo,0);put(cameo+4,kind);put(cameo+0xC,0u);original(originalText);
}
static std::u16string duration(unsigned frames,unsigned fps=0){
 auto digits=[](unsigned n){auto s=std::to_string(n);return std::u16string(s.begin(),s.end());};
 if(!fps)return digits(frames)+u" 帧";
 const unsigned centis=unsigned((uint64_t(frames)*100+fps/2)/fps);
 return digits(centis/100)+u"."+char16_t(u'0'+centis%100/10)+char16_t(u'0'+centis%10)+u" 秒";
}
static std::u16string output(uint32_t p){check(p&&p!=Original);const auto*text=reinterpret_cast<const wchar_t*>(uintptr_t(p));std::u16string result;
 for(unsigned i=0;i<256;++i){if(!text[i])return result;result.push_back(char16_t(text[i]));}check(false);return {};
}
static void expect(const std::u16string&suffix,unsigned id=1000){const auto before=std::vector<uint8_t>(memory.begin()+Original,memory.begin()+Original+512);const auto p=AssistBuildTimeTooltip(id,Original);
 check(output(p)==originalText+suffix);check(std::equal(before.begin(),before.end(),memory.begin()+Original));check(nativeCalls==1);
}
static void unchanged(bool queried=false,unsigned id=1000,unsigned source=Original){check(AssistBuildTimeTooltip(id,source)==source);check(nativeCalls==(queried?1u:0u));}
static void factory(int stage=0){put(cameo+0xC,Factory);put(Factory+0x6C,House);put(Factory+0x58,Product);put(Product,selected->vtable);put(Product+selected->typeOffset,selectedType);put(Product+0x21C,House);
 put(Factory+0x24,stage);put(Factory+0x38,10);put(Factory+0x34,10);put(Factory+0x2C,93);put(Factory+0x70,uint8_t(0));put(Factory+0x5C,uint8_t(0));}
static void loadImage(const char*path){
 std::ifstream in(path,std::ios::binary);std::vector<uint8_t>data((std::istreambuf_iterator<char>(in)),{});check(data.size()>0x100);
 auto u16=[&](size_t a){uint16_t x;check(a+2<=data.size());std::memcpy(&x,data.data()+a,2);return x;};
 auto u32=[&](size_t a){uint32_t x;check(a+4<=data.size());std::memcpy(&x,data.data()+a,4);return x;};
 const size_t pe=u32(0x3C),optional=pe+24,sections=optional+u16(pe+20);const uint32_t base=u32(optional+28);check(base==0x400000);
 for(unsigned i=0;i<u16(pe+6);++i){const size_t s=sections+i*40;const uint32_t va=base+u32(s+12),size=u32(s+16),offset=u32(s+20);check(uint64_t(va)+size<=memory.size()&&uint64_t(offset)+size<=data.size());std::memcpy(memory.data()+va,data.data()+offset,size);}
 pristine=memory;
}
struct NativeOutput{int kind,tab,raw,total;const char*name;};
#include "native-times.inc"
int main(int argc,char**argv){check(argc==2);loadImage(argv[1]);
 for(const auto&n:NativeOutputs){begin(n.name);reset(n.kind,n.tab);nativeRaw=n.raw;expect(u"\n建造："+duration(n.total));}
 const int kinds[]={7,7,40,16,3},tabs[]={0,1,3,2,3};
 for(unsigned i=0;i<5;++i){
  begin("remaining stage0");reset(kinds[i],tabs[i]);factory();expect(u"\n建造：865 帧\n剩余：534 帧");
  begin("remaining stage53");reset(kinds[i],tabs[i]);factory(53);expect(u"\n建造：865 帧\n剩余：4 帧");
  begin("ready takes priority");reset(kinds[i],tabs[i]);factory(54);put(Factory+0x70,uint8_t(1));put(Factory+0x5C,uint8_t(1));expect(u"\n建造：865 帧\n已就绪");
  begin("paused takes priority");reset(kinds[i],tabs[i]);factory(20);put(Factory+0x70,uint8_t(1));put(Factory+0x5C,uint8_t(1));expect(u"\n建造：865 帧\n已暂停");
  begin("funds wait");reset(kinds[i],tabs[i]);factory(20);put(Factory+0x5C,uint8_t(1));expect(u"\n建造：865 帧\n等待资金");
  begin("expired current deadline");reset(kinds[i],tabs[i]);factory(20);put(Factory+0x2C,80);expect(u"\n建造：865 帧\n剩余：331 帧");
  begin("new rate preserves running duration");reset(kinds[i],tabs[i]);factory(52);put(Factory+0x38,4);expect(u"\n建造：865 帧\n剩余：8 帧");
 }
 for(auto p:{std::pair<int,int>{-100,55},{0,55},{53,55},{54,55},{107,55},{108,109},{13770,13771},{999999,13771}}){begin("native rate clamp");reset(40,3);nativeRaw=p.first;expect(u"\n建造："+duration(p.second));}
 for(int session:{0,5,3,4,9})for(int speed=0;speed<6;++speed){begin("nominal speed");reset(16,2);put(0xA8B238,session);put(0xA8EB60,speed);const unsigned rates[]={60,60,30,20,15,12},network[]={60,45,30,20,15,12};expect(u"\n建造："+duration(865,session==9?0:session==3||session==4?network[speed]:rates[speed]));}
 begin("custom fps");reset(3,3);put(0xA8B238,0);put(0xA8EB60,1);singlefps::custom=144;factory(53);expect(u"\n建造：6.01 秒\n剩余：0.03 秒");
 for(int speed:{-1,6}){begin("unknown speed");reset();put(0xA8B238,0);put(0xA8EB60,speed);expect(u"\n建造：865 帧");}
 for(auto p:{std::pair<uint32_t,int>{0x24,-1},{0x24,55},{0x38,0},{0x38,256},{0x34,-1},{0x34,256},{0x2C,-1},{0x2C,101}}){begin("invalid factory timer");reset(40,3);factory();put(Factory+p.first,p.second);expect(u"\n建造：865 帧");}
 begin("foreign factory");reset();factory();put(Factory+0x6C,House+0x100);expect(u"\n建造：865 帧");
 begin("foreign product");reset();factory();put(Product+0x21C,House+0x100);expect(u"\n建造：865 帧");
 begin("other type product");reset(16,2);factory();put(Product+selected->typeOffset,selectedType+0x100);expect(u"\n建造：865 帧");
 begin("other kind product");reset(40,3);factory();put(Product,0x7E3EBCu);expect(u"\n建造：865 帧");
 for(int kind:{0,1,14,31,32,41}){begin("non production cameo");reset(kind,1);unchanged();}
 for(int tab:{-1,4}){begin("invalid tab");reset(40,tab);unchanged();}
 for(int count:{0,76}){begin("invalid capacity");reset(40,3,count);unchanged();}
 begin("last cameo of capacity75 with original text");reset(40,3,75,37,74);expect(u"\n建造：865 帧");
 begin("capacity75 cannot replace null native text");reset(40,3,75,37,74);unchanged(false,1000,0);
 begin("out of capacity");reset(40,3,75,37,74);unchanged(false,1001);
 begin("negative row");reset(40,3,10,-1);unchanged(false,1002);
 for(int id:{999,1120}){begin("non cameo tooltip id");reset();unchanged(false,id);}
 for(int index:{-1,1}){begin("invalid type index");reset();put(cameo,index);unchanged();}
 begin("bad type vector");reset();put(selected->array+8,0);unchanged();
 begin("unknown type vtable");reset(40,3);put(selectedType,0x123456u);unchanged();
 for(unsigned i=0;i<4;++i)for(unsigned slot:{0x2Cu,0x84u,0x88u}){begin("unknown receiver vtable slot");reset(buildtime::Types[i].cameoKind,3);put(selected->vtable+slot,0x123456u);unchanged();}
 for(unsigned slot:{0x2Cu,0x88u}){begin("unknown type vtable slot");reset(16,2);put(selected->typeVtable+slot,0x123456u);unchanged();}
 for(uint32_t at:{0x6F4852u,0x711EF1u,0x6F3272u,0x741490u,0x746E20u,0x748170u}){begin("unknown internal method bytes");reset(40,3);put(at,uint8_t(get<uint8_t>(at)^1));unchanged();}
 begin("partial receiver structural gate survives debug skip");reset(40,3);debugentries::Config skip{};check(debugentries::setSkip(skip,debugentries::id(debugentries::Kind::Call,0x6F47A0),true));debugentries::applyPolicy(skip);put(0x6F4852,uint8_t(get<uint8_t>(0x6F4852)^1));unchanged();
 begin("unreadable query code");reset(16,2);readFailure=0x6F4870;unchanged();
 begin("unreadable original text");reset(16,2);readFailure=Original+4;unchanged(true);
 begin("empty original text");reset();original(u"");unchanged(true);
 begin("translated original too long");reset();original(std::u16string(180,u'字'));unchanged(true);
 begin("UTF16 and maximum accepted original");reset();originalText=std::u16string(177,u'字')+u"\U0001F600";original(originalText);factory();expect(u"\n建造：865 帧\n剩余：534 帧");originalText=u"战车\n$1000";
 const std::vector<std::function<void()>> stale={
  []{put(Sidebar+0x539C,2);},[]{put(strip+0x44,1);},[]{put(strip+0x54,11);},[]{put(cameo,1);},[]{put(cameo+4,16);},
  []{put(get<uint32_t>(selected->array+4),selectedType+0x100);},[]{put(selectedType,0x123456u);},[]{put(0xA83D4C,House+0x100);},[]{put(0xA8ED84,101);},[]{lastHouse+=0x100;},[]{++match;}
 };
 for(const auto&mutate:stale){begin("source changes during native callback");reset(40,3);nativeMutation=mutate;unchanged(true);}
 const std::vector<std::function<void()>> staleProduct={
  []{put(cameo+0xC,Factory+0x100);},[]{put(Factory+0x6C,House+0x100);},[]{put(Factory+0x58,OtherProduct);},
  []{put(Product,0x7E3EBCu);},[]{put(Product+selected->typeOffset,selectedType+0x100);},[]{put(Product+0x21C,House+0x100);}
 };
 for(const auto&mutate:staleProduct){begin("factory product changes before final identity");reset(40,3);factory();afterAddress=Factory+0x2C;afterRead=mutate;unchanged(true);}
 begin("changed page does not reread retired product");reset(40,3);factory();afterAddress=Factory+0x2C;afterRead=[]{put(Sidebar+0x539C,2);readFailure=Product;};unchanged(true);
 begin("late page switch after native call");reset(16,2);afterAddress=Original;afterRead=[]{put(Sidebar+0x539C,3);};unchanged(true);
 begin("focus guard");reset();shared.foreground=false;unchanged();
 begin("heartbeat guard");reset();now=1501;unchanged();
 begin("match guard");reset();++snap.match;unchanged();
 begin("feature guard");reset();shared.buildTimeTooltip=false;unchanged();
 begin("failure guard");reset();AssistBuildTimeTooltipFailed();unchanged();buildTimeTooltipTick();check(!buildTimeTooltipActive&&shared.buildTimeTooltipStatus==-3);
 begin("original compatibility and ownership");reset();buildTimeTooltipActive=false;buildTimeTooltipTick();check(buildTimeTooltipActive&&shared.buildTimeTooltipStatus==1&&claimed);shared.buildTimeTooltip=false;buildTimeTooltipTick();check(!buildTimeTooltipActive&&shared.buildTimeTooltipStatus==0&&!claimed);
 begin("existing incompatible entry gate");reset();buildTimeTooltipActive=false;put(0x6F47A0,uint8_t(0xE9));buildTimeTooltipTick();check(!buildTimeTooltipActive&&shared.buildTimeTooltipStatus==-2);
 std::printf("{\"passed\":true,\"checks\":%u,\"scenarios\":%u,\"native_call_seam_invocations\":%u,\"utf16_wchar_bytes\":%zu}\n",checks,scenarios,totalNativeCalls,sizeof(wchar_t));
}
