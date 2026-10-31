#include "../source/crc_shadow_restore.h"
using namespace assist::crcshadow;
struct Memory {
 enum Write { Success, FailBefore, FailAfter } mode=Success;
 uint32_t scenario=0x40000,items=0x30000,local=0x21000,localID=71,house=0x22000,houseID=72;
 int session=3,count=2,index=1,writes=0;uint8_t byte=1;uint32_t unreadable=0;
 constexpr bool read(uint32_t at,uint32_t*out,size_t n){
  if(at==unreadable||n!=4)return false;
  if(at==0xA8B230)*out=scenario;else if(at==0xA8022C)*out=items;
  else if(at==0xA83D4C)*out=local;else if(at==local+0x10)*out=localID;
  else if(at==items+4)*out=house;else if(at==0x22010)*out=houseID;
  else return false;return true;
 }
 constexpr bool read(uint32_t at,int*out,size_t n){
  if(at==unreadable||n!=4)return false;
  if(at==0xA8B238)*out=session;else if(at==0xA80238)*out=count;
  else if(at==0x22030)*out=index;else return false;return true;
 }
 constexpr bool read(uint32_t at,uint8_t*out,size_t n){
  if(at==unreadable||at!=0x22241||n!=1)return false;*out=byte;return true;
 }
 constexpr bool write(uint32_t at,const uint8_t*value,size_t n){
  if(at!=0x22241||n!=1||mode==FailBefore)return false;
  byte=*value;++writes;return mode!=FailAfter;
 }
};
struct Fixture {
 Memory memory{};Model model{};World world{7,0x40000,0x30000,0x21000,71,3};Identity id{7,0x22000,72,1};
 constexpr Fixture(){model.frame(7,10);model.beginTool(id,0,1);}
 constexpr Restoration restore(uint32_t epoch=7,int frame=11){return restoreData(model,world,epoch,frame,10,
  [this](uint32_t at,auto*out,size_t n){return memory.read(at,out,n);},
  [this](uint32_t at,const uint8_t*value,size_t n){return memory.write(at,value,n);});}
};
constexpr bool restored(){Fixture f;auto r=f.restore();return r.complete&&!r.conflict&&f.memory.byte==0&&f.memory.writes==1&&f.model.find(f.id)->original==f.model.find(f.id)->display;}
constexpr bool readFailure(uint32_t at){Fixture f;f.memory.unreadable=at;auto r=f.restore();
 if(r.complete||r.retained!=1||f.memory.writes||f.model.find(f.id)->original!=0||f.model.find(f.id)->display!=1)return false;
 f.memory.unreadable=0;r=f.restore();return r.complete&&f.memory.byte==0&&f.memory.writes==1;}
constexpr bool writeFailure(bool after){Fixture f;f.memory.mode=after?Memory::FailAfter:Memory::FailBefore;auto r=f.restore();
 if(r.complete||r.retained!=1||f.model.find(f.id)->display!=1||f.model.find(f.id)->original!=0)return false;
 f.memory.mode=Memory::Success;r=f.restore();return r.complete&&!r.conflict&&f.memory.byte==0&&f.memory.writes==1;}
constexpr bool alreadyOriginal(){Fixture f;f.memory.byte=0;auto r=f.restore();return r.complete&&!f.memory.writes&&f.model.find(f.id)->display==0;}
constexpr bool foreign(){Fixture f;f.memory.byte=2;auto r=f.restore();
 if(r.complete||!r.conflict||r.retained!=1||f.memory.writes||f.memory.byte!=2)return false;
 f.memory.byte=0;return f.restore().complete&&!f.memory.writes;}
constexpr bool retiredWorld(){Fixture f;return f.restore(8).complete&&!f.memory.writes&&f.memory.byte==1;}
constexpr bool retiredHouse(){Fixture f;f.memory.houseID=73;return f.restore().complete&&!f.memory.writes&&f.memory.byte==1;}
constexpr bool rewind(){Fixture f;return f.restore(7,9).complete&&!f.memory.writes&&f.memory.byte==1;}
constexpr bool empty(){Fixture f;f.model.clear();f.memory.unreadable=0xA8B230;return f.restore().complete&&!f.memory.writes;}
constexpr bool nativeWrite(){Fixture f;f.model.naturalWrite(f.id,1);f.memory.unreadable=0xA8B230;return f.restore().complete&&!f.memory.writes&&f.memory.byte==1;}
constexpr bool unknown(){Fixture f;f.model.unknownWrite(f.id);f.memory.byte=2;return f.restore().complete&&!f.memory.writes&&f.model.crcValue(f.id,2)==2;}
constexpr bool invalidVector(){Fixture f;f.memory.count=-1;auto r=f.restore();return !r.complete&&r.retained==1&&!f.memory.writes;}
static_assert(restored(),"restore owned byte before the caller may unhook/clear");
static_assert(readFailure(0xA8B230),"world read failure cannot authorize dropping the shadow");
static_assert(readFailure(0x30004),"House membership read failure retains the shadow");
static_assert(readFailure(0x22030),"House identity read failure is not retirement");
static_assert(readFailure(0x22241),"field read failure retains the original");
static_assert(writeFailure(false),"failed write remains retryable");
static_assert(writeFailure(true),"failed verification after restoration is idempotent");
static_assert(alreadyOriginal(),"already-restored byte needs no write");
static_assert(foreign(),"foreign byte is not overwritten or forgotten");
static_assert(retiredWorld(),"old epoch cannot write a new world");
static_assert(retiredHouse(),"reused House cannot receive old data");
static_assert(rewind(),"rewound world cannot receive pre-load values");
static_assert(empty(),"no owned data needs no world-dependent rollback");
static_assert(nativeWrite(),"natural same-value write is retained");
static_assert(unknown(),"unknown data never gets a fabricated CRC value");
static_assert(invalidVector(),"invalid vector does not prove House retirement");
int main(){}
