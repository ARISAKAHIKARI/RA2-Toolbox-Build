#pragma once
#include <cstdint>
#include <cstdlib>
namespace clickinput {
struct Burst {
 int left=0;bool armed=false;int x=0,y=0;uint32_t due=0;
 void cancel(){left=0;armed=false;}
 void down(int px,int py,bool eligible){cancel();armed=eligible;x=px;y=py;}
 void up(int px,int py,bool eligible,uint32_t now,unsigned gap){if(armed&&eligible&&std::abs(px-x)<=4&&std::abs(py-y)<=4){left=4;due=now+gap;}armed=false;}
 bool ready(uint32_t now)const{return left>0&&int32_t(now-due)>=0;}
 void sent(uint32_t now,unsigned gap){if(left)--left;due=now+gap;}
};

}
