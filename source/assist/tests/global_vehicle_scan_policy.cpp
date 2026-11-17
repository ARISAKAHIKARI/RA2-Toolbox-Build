#include "../source/global_vehicle_scan_policy.h"
#include <cassert>
#include <cstdio>
#include <vector>

int main(){
 using assist::globalvehicle::Cursor;
 Cursor cursor;
 assert(cursor.budget(0)==0&&cursor.budget(10)==10);
 assert(cursor.index(0,0)==0);
 cursor.advance(0,0);
 assert(cursor.next==0);
 // A 16K cohort is completely covered with bounded 64-probe slices.
 constexpr unsigned actors=16384;
 std::vector<unsigned char>observed(actors,0);
 for(unsigned frame=0;frame<actors/Cursor::MaxProbesPerFrame;++frame){
  const unsigned count=cursor.budget(actors);
  assert(count==Cursor::MaxProbesPerFrame);
  for(unsigned i=0;i<count;++i){
   const unsigned at=cursor.index(actors,i);
   assert(at<actors&&observed[at]==0);
   observed[at]=1;
  }
  cursor.advance(actors,count);
 }
 for(const auto value:observed)assert(value==1);
 assert(cursor.next==0);
 // A partial slice must not advance over candidates not examined when the
 // native admission queue runs out of space partway through.
 cursor.advance(101,3);
 assert(cursor.next==3&&cursor.index(101,0)==3);
 cursor.advance(101,0);
 assert(cursor.next==3);
 cursor.advance(101,64);
 assert(cursor.next==67);
 // A roster that shrinks/reorders between frames cannot cause out-of-range
 // access; every pointer still undergoes live ID/House/mission validation.
 assert(cursor.index(3,0)==1&&cursor.index(3,2)==0);
 cursor.advance(3,3);
 assert(cursor.next==1);
 cursor.reset();
 assert(cursor.next==0);
 std::puts("PASS capped round-robin global candidate slices, full 16K coverage and bounded cursor recovery");
}
