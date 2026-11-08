#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace cursorondemand {
constexpr size_t MaxPairs=512;
enum class Mode {Pairs,All,Preview};

// One owner thread records observed, already-authorized animation edges.
// Failed/cancelled work stays quiet until an explicit retry or a new source.
// The draw path only touches these bounded containers; it never runs Python,
// opens a file, computes optical flow, or waits for a worker.
class Queue {
 std::set<unsigned> pending_,attempted_;bool blocked_=false;
public:
 void reset(){pending_.clear();attempted_.clear();blocked_=false;}
 void block(){blocked_=true;}
 bool blocked()const{return blocked_;}
 bool empty()const{return pending_.empty();}
 size_t size()const{return pending_.size();}
 template<class Ready>void discardReady(Ready ready){for(auto i=pending_.begin();i!=pending_.end();)if(ready(*i))i=pending_.erase(i);else ++i;}
 bool request(unsigned pair,size_t frames){
  const unsigned a=pair>>16,b=pair&65535;
  if(blocked_||a==b||a>=frames||b>=frames||attempted_.count(pair)||pending_.count(pair)||
     pending_.size()+attempted_.size()>=MaxPairs)return false;
  pending_.insert(pair);return true;
 }
 std::vector<unsigned> take(size_t maximum=4){
  std::vector<unsigned> out;if(blocked_)return out;
  out.reserve(std::min(maximum,pending_.size()));
  while(!pending_.empty()&&out.size()<maximum){auto i=pending_.begin();out.push_back(*i);attempted_.insert(*i);pending_.erase(i);}
  return out;
 }
};

inline std::string requestJson(Mode mode,const std::vector<unsigned>&pairs={}){
 std::string out="{\"schema\":1,\"mode\":\"";
 out+=mode==Mode::Pairs?"pairs":mode==Mode::Preview?"preview":"all";out+='"';
 if(mode==Mode::Pairs){out+=",\"pairs\":[";bool comma=false;
  for(unsigned pair:pairs){if(comma)out+=',';comma=true;out+='[';out+=std::to_string(pair>>16);out+=',';out+=std::to_string(pair&65535);out+=']';}out+=']';}
 return out+"}\n";
}

// A versioned content key names an immutable local source, not a game pointer
// or a mutable style folder. Materialization also compares the complete input
// bytes before reusing the directory; the key alone never authorizes reuse.
template<class Bytes>inline std::wstring sourceKey(const Bytes&source,const Bytes&palette,const Bytes&groups){
 uint64_t a=1469598103934665603ull,b=1099511628211ull;
 auto byte=[&](uint8_t value){a=(a^value)*1099511628211ull;b=(b^value)*14029467366897019727ull;};
 byte(1);
 for(const Bytes*part:{&source,&palette,&groups}){
  const uint64_t n=part->size();for(unsigned i=0;i<8;++i)byte(uint8_t(n>>(8*i)));
  for(uint8_t value:*part)byte(value);
 }
 const wchar_t hex[]=L"0123456789abcdef";std::wstring out;
 for(uint64_t value:{a,b})for(int i=15;i>=0;--i)out+=hex[(value>>(i*4))&15];
 return out;
}
}
