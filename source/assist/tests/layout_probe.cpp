#include "../../external_reader/source/layout_probe.h"
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <vector>
using namespace external::layout;
struct Memory {
 std::unordered_map<uint32_t,std::vector<unsigned char>> bytes;
 template<class T>void set(uint32_t addr,T v){
  auto&b=bytes[addr];b.resize(sizeof v);std::memcpy(b.data(),&v,sizeof v);
 }
 bool operator()(uint32_t addr,void*out,size_t n)const{
  auto it=bytes.find(addr);
  if(it==bytes.end()||it->second.size()<n)return false;
  std::memcpy(out,it->second.data(),n);return true;
 }
};
int main(){
 Memory m;
 assert(inspect(m).stage==Stage::UnreadableGameFlag);
 m.set<uint8_t>(0xA8E9A0,0);
 assert(inspect(m).stage==Stage::WaitingForMatch);
 m.set<uint8_t>(0xA8E9A0,1);
 assert(inspect(m).stage==Stage::UnreadableLocalHouse);
 m.set<uint32_t>(0xA83D4C,0);
 assert(inspect(m).stage==Stage::InvalidLocalHouse);
 m.set<uint32_t>(0xA83D4C,0xA80000);
 assert(inspect(m).stage==Stage::UnreadableFrame);
 m.set<int32_t>(0xA8ED84,-1);
 assert(inspect(m).stage==Stage::InvalidFrame);
 m.set<int32_t>(0xA8ED84,20);
 assert(inspect(m).stage==Stage::Ready);
}
