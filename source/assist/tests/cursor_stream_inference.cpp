#include "../../cursor/source/stream_inference.h"
#include <cassert>
#include <cstdio>
#include <vector>

static void fill(std::vector<unsigned char>&pixels,int width,int x0,int y0,int x1,int y1){
 for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)pixels[size_t(y)*width+x]=1;
}
int main(){
 for(int size:{55,128,256,512}){
  std::vector<unsigned char> full(size_t(size)*size,1);
  assert(cursorstream::plausible(full.data(),full.data(),size,size,size,size));
 }
 constexpr int size=512;
 std::vector<unsigned char>a(size_t(size)*size),near(a.size()),far(a.size()),empty(a.size());
 fill(a,size,0,0,256,256);fill(near,size,60,0,316,256);fill(far,size,256,256,512,512);
 assert(cursorstream::plausible(a.data(),near.data(),size,size,size,size));
 assert(!cursorstream::plausible(a.data(),far.data(),size,size,size,size));
 assert(!cursorstream::plausible(a.data(),empty.data(),size,size,size,size));
 assert(!cursorstream::plausible(nullptr,a.data(),size,size,size,size));
 assert(!cursorstream::plausible(a.data(),a.data(),size,size,size-1,size));
 cursorstream::PairShapeGate gate;
 unsigned scans=0;
 auto scan=[&]{++scans;return cursorstream::plausible(a.data(),near.data(),size,size,size,size);};
 for(int i=0;i<10000;++i)assert(gate.get(10,scan));
 assert(scans==1);
 assert(gate.get(11,scan)&&scans==2);
 // The same frame number in a newly loaded resource must not reuse its old
 // decision. Production resets the gate at both original-asset replacements.
 gate.reset();
 assert(!gate.get(11,[&]{++scans;return cursorstream::plausible(a.data(),far.data(),size,size,size,size);})&&scans==3);
 for(int i=0;i<10000;++i)assert(!gate.get(11,[&]{++scans;return true;}));
 assert(scans==3);
 cursorstream::Detector detector;
 detector.observe(1,100);detector.observe(2,160);detector.observe(3,220);
 assert(detector.predicts(3,5));
 detector.observe(-1,240);detector.observe(3,250);
 assert(detector.predicts(3,5));
 detector.observe(5,280);assert(!detector.predicts(5,8));
 detector.reset();assert(!detector.predicts(5,8));
 std::printf("cursor stream: 55/128/256/512 shape gates valid; 10000 same-frame evaluations -> 1 scan; new pair/reset/rejected pair covered\n");
}
