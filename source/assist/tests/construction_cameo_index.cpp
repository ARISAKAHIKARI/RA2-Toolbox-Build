#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
int main(){
 auto nativeKey=[](int kind,int index){return(uint64_t(uint32_t(kind))<<32)|uint32_t(index);};
 std::array<uint64_t,75> indexed{};
 constexpr int kinds[]={3,7,16,40};
 for(int size=0;size<=75;++size){
  for(int i=0;i<size;++i)indexed[i]=nativeKey(kinds[(i*7)%4],(i*31)%97);
  auto unsorted=indexed;
  std::sort(indexed.begin(),indexed.begin()+size);
  for(int kind: kinds)for(int index=0;index<130;++index){
   bool linear=false;for(int i=0;i<size;++i)linear|=unsorted[i]==nativeKey(kind,index);
   bool binary=std::binary_search(indexed.begin(),indexed.begin()+size,nativeKey(kind,index));
   assert(linear==binary);
  }
 }
 std::puts("PASS 0..75 native cameo entries sorted-key lookup matches linear producer");
}
