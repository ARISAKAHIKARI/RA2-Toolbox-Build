#define main existingRoundTransitionRegression
#include "round_transition.cpp"
#undef main
#include <string>

namespace {
unsigned checks=0,failures=0,scenarios=0,recovered=0;
const char* currentCase="";
void check(bool ok,const char*what){++checks;if(!ok){++failures;std::cerr<<currentCase<<": "<<what<<"\n";}}
struct Fault {
 uint32_t address=0;size_t bytes=0;unsigned skip=0,hits=0;bool fired=false;
 bool rejects(uint32_t p,size_t n){
  if(fired||p!=address||(bytes&&n!=bytes))return false;
  if(hits++<skip)return false;fired=true;return true;
 }
};
struct FaultRead {
 Memory* memory;Fault* fault;
 bool operator()(uint32_t p,void*out,size_t n){return !fault->rejects(p,n)&&memory->read(p,out,n);}
 bool fresh(uint32_t p,void*out,size_t n){return memory->fresh(p,out,n);}
};
constexpr uint32_t Second=0x220000,Vehicle=0x230000,InfantryItems=0x300100;
struct Case {
 Fixture fixture;Fault fault;
 std::unique_ptr<um::Sampler<FaultRead,Resolve>> sampler;
 Case(){
  auto&m=fixture.m;
  std::memcpy(m.data.data()+Second,m.data.data()+Infantry,4096);
  std::memcpy(m.data.data()+Vehicle,m.data.data()+Infantry,4096);
  std::memcpy(m.data.data()+Second+36,"E2",3);std::memcpy(m.data.data()+Second+1750,"E2ICON",7);
  std::memcpy(m.data.data()+Vehicle+36,"MTNK",5);std::memcpy(m.data.data()+Vehicle+1750,"TNKICON",8);
  m.vector(0xA8E348,InfantryItems,{Infantry,Second});m.vector(0xA83CE0,0x300300,{Vehicle});
  for(int i=0;i<2;++i){auto h=i?Enemy:Self;auto infantryCounts=uint32_t(0x320000+i*0x1000),vehicleCounts=uint32_t(0x330000+i*0x1000);
   m.put(h+um::OwnedOffsets[0]+4,infantryCounts);m.put(h+um::OwnedOffsets[0]+8,2);m.put(infantryCounts,5+i);m.put(infantryCounts+4,7+i);
   m.put(h+um::OwnedOffsets[1]+4,vehicleCounts);m.put(h+um::OwnedOffsets[1]+8,1);m.put(vehicleCounts,9+i);
  }
  sampler=std::make_unique<um::Sampler<FaultRead,Resolve>>(FaultRead{&m,&fault},Resolve{});
 }
 const um::Snapshot& sample(uint32_t tick){fixture.m.bookends=0;return sampler->sample(tick,true);}
 bool complete(const um::Snapshot&s,bool firstFiltered=false){
  if(s.status!=1||s.playerCount!=2||s.typeCount!=(firstFiltered?2:3))return false;
  const int offset=firstFiltered?0:1;
  if(!firstFiltered&&(std::string(s.types[0].id)!="E1"||s.players[0].cells[0].owned!=5))return false;
  return std::string(s.types[offset].id)=="E2"&&s.players[0].cells[offset].owned==7&&
   std::string(s.types[offset+1].id)=="MTNK"&&s.players[0].cells[offset+1].owned==9;
 }
};
struct Field {const char*name;uint32_t address;size_t bytes;unsigned skip;bool filtered;};
}
int main(){
 const Field fields[]={
  {"cameo",Infantry+1750,24,1,false},{"id",Infantry+36,23,0,false},
  {"level",Infantry+1588,4,1,false},{"owners",Infantry+1740,4,1,false},
  {"insignificant",Infantry+562,1,1,true},{"spawned",Infantry+3412,1,1,true},
  {"vector_items",0xA8E348+4,4,2,false},{"vector_capacity",0xA8E348+8,4,1,false},
  {"vector_count",0xA8E348+16,4,2,false},{"vector_entries",InfantryItems,8,0,false}
 };
 for(bool warm:{false,true})for(const auto&field:fields){
  Case c;currentCase=field.name;++scenarios;
  if(warm){check(c.complete(c.sample(100)),"initial catalogue must be complete");
   // A real metadata change forces rebuilding an already populated table.
   c.fixture.m.put<int>(Second+1588,2);
  }
  if(field.filtered)c.fixture.m.put<uint8_t>(field.address,1);
  c.fault={field.address,field.bytes,field.skip};
  const auto&failed=c.sample(700);
  check(c.fault.fired,"failure must occur after the successful catalogue stamp");
  check(failed.status==0&&failed.typeCount==0&&failed.playerCount==0,"partial catalogue must not be published as complete");
  check(c.complete(c.sample(950),field.filtered),"the next healthy sample must recover every eligible type");
  const bool later=c.complete(c.sample(1600),field.filtered);check(later,"unchanged metadata must not retain an incomplete table");
  recovered+=later;
 }
 {Case c;currentCase="legal_filtered_and_empty_arrays";++scenarios;
  check(c.complete(c.sample(100)),"healthy baseline");
  std::memcpy(c.fixture.m.data.data()+Infantry+1750,"none",5);
  check(c.complete(c.sample(700),true),"known absent cameo is a legal filter");
  c.fixture.m.vector(0xA8E348,InfantryItems,{});const auto&s=c.sample(1300);
  check(s.status==1&&s.typeCount==1&&std::string(s.types[0].id)=="MTNK"&&s.players[0].cells[0].owned==9,"known empty array must not reject other categories");
 }
 std::cout<<"{\"checks\":"<<checks<<",\"failures\":"<<failures<<",\"scenarios\":"<<scenarios<<",\"recovered_catalogues\":"<<recovered<<"}\n";
 return failures?1:0;
}
