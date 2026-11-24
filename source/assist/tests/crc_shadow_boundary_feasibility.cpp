#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include "../../research/crc_shadow_boundary_candidate.h"

extern "C" void RuntimeScopedCall(void*context,void(*body)(void*),void(*cleanup)(void*)){
 try{body(context);}catch(...){cleanup(context);throw;}
 cleanup(context);
}

struct Identity {
 uint32_t epoch=0,id=0;
 unsigned index=0;
 bool operator==(const Identity&o)const{return epoch==o.epoch&&id==o.id&&index==o.index;}
};
struct Memory {
 uint32_t house=0x20000,epoch=1,id=7;
 unsigned index=0,reads=0,writes=0,reveals=0,codePatchWrites=0;
 uint8_t byte=0;
 bool alive=true,writeFail=false;
 int failReadAt=-1;
 bool identity(uint32_t p,Identity*out){
  if(!alive||p!=house)return false;
  *out={epoch,id,index};return true;
 }
 bool read(uint32_t p,uint8_t*out){
  ++reads;if(int(reads)==failReadAt||p!=house+0x241)return false;
  *out=byte;return true;
 }
 bool write(uint32_t p,uint8_t value){
  if(writeFail||p!=house+0x241)return false;
  byte=value;++writes;return true;
 }
 auto getIdentity(){return [this](uint32_t p,Identity*out){return identity(p,out);};}
 auto reader(){return [this](uint32_t p,uint8_t*out){return read(p,out);};}
 auto writer(){return [this](uint32_t p,uint8_t value){return write(p,value);};}
};
using namespace assist::crcshadowboundary;
static unsigned tests=0;
static void check(bool value,const char*name){
 if(!value){std::fprintf(stderr,"FAIL: %s\n",name);std::abort();}
 ++tests;
}
int main(){
 {
  Memory m;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;m.byte=1;});
  check(r==Result::Restored&&m.byte==0&&m.writes==1&&!tx.armed&&m.codePatchWrites==0,
        "0->1 tool reveal restores original without code patches");
 }
 {
  Memory m;m.byte=1;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;m.byte=1;});
  check(r==Result::AlreadyOriginal&&m.byte==1&&m.writes==0,
        "original one is not rewritten");
 }
 {
  Memory m;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;});
  check(r==Result::AlreadyOriginal&&m.byte==0&&m.writes==0,
        "native no-op reveal remains a no-op");
 }
 {
  Memory m;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;m.byte=1;});
  check(r==Result::Restored,"tool reveal cleanup completes");
  m.byte=1;uint8_t crc=0;
  check(m.read(m.house+0x241,&crc)&&crc==1,
        "later natural same-value write is visible to native CRC");
 }
 {
  Memory m;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;m.byte=1;m.id=8;});
  check(r==Result::IdentityChanged&&m.byte==1&&m.writes==0&&!tx.armed,
        "reused House identity is never overwritten");
 }
 {
  Memory m;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;m.byte=2;});
  check(r==Result::Conflict&&m.byte==2&&tx.armed&&m.writes==0,
        "unexpected write fails closed and retains ownership");
  check(begin(tx,m.house,m.getIdentity(),m.reader())==Result::Busy,
        "new reveal is blocked while conflict is unresolved");
 }
 {
  Memory m;m.writeFail=true;Transaction<Identity>tx;
  auto r=run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{++m.reveals;m.byte=1;});
  check(r==Result::WriteFailed&&tx.armed&&m.byte==1,
        "failed restoration retains retry state");
  m.writeFail=false;r=finish(tx,m.getIdentity(),m.reader(),m.writer());
  check(r==Result::Restored&&m.byte==0&&!tx.armed,
        "retry completes retained restoration");
 }
 {
  Memory m;Transaction<Identity>tx;m.failReadAt=1;
  check(begin(tx,m.house,m.getIdentity(),m.reader())==Result::ReadFailed&&!tx.armed,
        "pre-reveal read failure rejects the operation");
 }
 {
  Memory m;Transaction<Identity>tx;
  check(begin(tx,m.house,m.getIdentity(),m.reader())==Result::Armed,
        "transaction captures original identity and byte");
  m.byte=1;m.failReadAt=int(m.reads+1);
  check(finish(tx,m.getIdentity(),m.reader(),m.writer())==Result::ReadFailed&&tx.armed,
        "post-reveal read failure retains the transaction");
  m.failReadAt=-1;
  check(finish(tx,m.getIdentity(),m.reader(),m.writer())==Result::Restored&&m.byte==0,
        "post-reveal read retry restores original");
 }
 {
  Memory m;Transaction<Identity>tx;
  check(begin(tx,m.house,m.getIdentity(),m.reader())==Result::Armed,
        "verify-failure transaction begins");
  m.byte=1;m.failReadAt=int(m.reads+2);
  check(finish(tx,m.getIdentity(),m.reader(),m.writer())==Result::VerifyFailed&&
        tx.armed&&m.byte==0,
        "write succeeded but failed readback retains idempotent ownership");
  m.failReadAt=-1;
  check(finish(tx,m.getIdentity(),m.reader(),m.writer())==Result::AlreadyOriginal&&!tx.armed,
        "readback retry recognizes already-restored original");
 }
 {
  Memory m;Transaction<Identity>tx;bool threw=false;
  try{
   run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{
    ++m.reveals;m.byte=1;throw std::runtime_error("synthetic unwind after store");
   });
  }catch(const std::runtime_error&){threw=true;}
  check(threw&&m.byte==0&&m.writes==1&&!tx.armed,
        "finally cleanup restores after exceptional native Reveal");
 }
 {
  Memory m;Transaction<Identity>tx;bool threw=false;
  try{
   run(tx,m.house,m.getIdentity(),m.reader(),m.writer(),[&]{
    ++m.reveals;throw std::runtime_error("synthetic unwind before store");
   });
  }catch(const std::runtime_error&){threw=true;}
  check(threw&&m.byte==0&&m.writes==0&&!tx.armed,
        "finally cleanup is idempotent when Reveal did not write");
 }
 {
  Memory m;m.byte=3;Transaction<Identity>tx;
  check(begin(tx,m.house,m.getIdentity(),m.reader())==Result::InvalidOriginal&&!tx.armed,
        "non-boolean original is rejected");
 }
 std::printf("CRC single-boundary feasibility: %u checks PASS (synthetic transaction model; no game/MP execution)\n",tests);
}
