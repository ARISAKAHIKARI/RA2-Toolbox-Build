#include "../source/secondary_radar_layout.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

int main(){
 auto place=[](int x,int y,int width,double aspect){return secondaryradar::place(-1920,0,0,1080,x,y,width,aspect);};
 auto bottomRight=place(10000,10000,600,2);
 assert(bottomRight.w==600&&bottomRight.h==301);
 assert(bottomRight.x==-612&&bottomRight.y==767);
 auto upperLeft=place(0,0,600,2);
 assert(upperLeft.x==-1908&&upperLeft.y==12);
 auto middle=place(5000,5000,600,2);
 assert(middle.x>upperLeft.x&&middle.x<bottomRight.x&&middle.y>upperLeft.y&&middle.y<bottomRight.y);
 assert(place(-50,-1,600,2).x==upperLeft.x&&place(15000,15000,600,2).x==bottomRight.x);
 auto wider=place(10000,10000,1000,2);
 assert(wider.w==1000&&wider.h==501&&wider.x==-(1000+12));
 // The default 320 px near-square map remains below the existing upper-right
 // tracking group (at most two-thirds of this 1920x1080 work area).
 radarlayout::Box watchGroup{-652,12,640,696};
 for(double aspect:{1.0,1.2}){
  auto defaultMap=place(10000,10000,320,aspect);
  assert(defaultMap.w==320&&defaultMap.y>=748);
  assert(!radarlayout::overlap(defaultMap,watchGroup,8));
 }
 auto large=place(10000,10000,4096,2);
 assert(large.w==1896&&large.x==-1908&&large.y>=12&&large.y+large.h<=1068);
 auto tall=place(8000,7000,600,0.5);
 assert(tall.w<600&&tall.h<=1056&&tall.x>=-1908&&tall.x+tall.w<=-12);
 assert(std::abs(double(tall.w-2)/(tall.h-2)-0.5)<0.001);
 auto small=secondaryradar::place(100,-70,108,-64,10000,10000,4096,2);
 assert(small.w>=3&&small.h>=3&&small.x>=100&&small.y>=-70&&small.x+small.w<=108&&small.y+small.h<=-64);
 auto invalid=secondaryradar::place(0,0,1,100,100,100,600,2);assert(invalid.w==0);
 invalid=secondaryradar::place(std::numeric_limits<int>::min(),0,std::numeric_limits<int>::max(),100,50,50,600,2);assert(invalid.w==0);
 auto badAspect=place(10000,10000,600,std::numeric_limits<double>::quiet_NaN());assert(badAspect.w==600&&badAspect.h==600);
 auto captured=secondaryradar::capture(-1920,0,0,1080,middle,600);
 auto restored=place(captured.x,captured.y,captured.width,2);
 assert(std::abs(restored.x-middle.x)<=1&&std::abs(restored.y-middle.y)<=1&&captured.width==600);
 auto constrained=secondaryradar::capture(-1920,0,0,1080,tall,600);
 assert(constrained.width==600&&constrained.x>=0&&constrained.x<=10000&&constrained.y>=0&&constrained.y<=10000);
 for(int monitorWidth:{8,320,1280,1920,3840})for(int monitorHeight:{6,240,720,1080,2160})
  for(double ratio:{0.2,0.5,1.0,2.0,5.0})for(int px:{0,5000,10000})for(int py:{0,5000,10000}){
   int left=-monitorWidth,top=-160;
   auto b=secondaryradar::place(left,top,0,top+monitorHeight,px,py,4096,ratio);
   assert(b.w>=3&&b.h>=3&&b.x>=left&&b.y>=top&&b.x+b.w<=0&&b.y+b.h<=top+monitorHeight);
  }
 std::cout<<"PASS independent secondary radar placement: percent anchors, separate pixel size, aspect fit, signed origins and small screens\n";
}
