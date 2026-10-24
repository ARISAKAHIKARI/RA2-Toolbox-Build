#include "../../external_reader/source/watch_slots.h"
#include <cassert>
#include <iostream>
#include <cstring>
using namespace external::watch;
static std::vector<uint8_t> memory(0x1000000);
static bool read(uint32_t p,void*out,size_t n){if(uint64_t(p)+n>memory.size())return false;memcpy(out,memory.data()+p,n);return true;}
template<class T>static void put(uint32_t p,T v){memcpy(memory.data()+p,&v,sizeof v);}
static void vec(uint32_t p,uint32_t storage,const std::vector<uint32_t>&items){put(p+4,storage);put(p+8,int(items.size()));put(p+16,int(items.size()));memcpy(memory.data()+storage,items.data(),items.size()*4);}
static Target unit(int index,int x,int y){uint32_t p=0x10000+index*0x1000;put(p,0x7F5C70u);put(p+0x10,index+1);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,x);put(p+0xA0,y);return {p,uint32_t(index+1)};}
int main(){
 auto a=unit(0,30*256,30*256),b=unit(1,34*256,30*256),c=unit(2,35*256,30*256),unselected=unit(3,30*256+1,30*256);
 vec(0xA8EC78,0x200000,{a.pointer,b.pointer,c.pointer,unselected.pointer});vec(0xA8ECB8,0x210000,{a.pointer,b.pointer,c.pointer});put(unselected.pointer+0x431,uint8_t(1));
 auto captured=capture(read);assert(captured.target==a&&captured.members.size()==3); // hovered outsider cannot change group
 uint32_t keys[3]={87,86,87};Track slots[3];int slot=-1;
 assert(firstBinding(keys,0)&&firstBinding(keys,1)&&!firstBinding(keys,2));
 assert(add(slots,keys,87,captured,slot)==AddResult::Added&&slot==0);
 assert(add(slots,keys,87,captured,slot)==AddResult::AlreadyTracked&&slot==0);
 auto other=captured;other.target=b;assert(add(slots,keys,87,other,slot)==AddResult::Added&&slot==2&&!slots[1]);
 other.target=c;assert(add(slots,keys,87,other,slot)==AddResult::Full&&!slots[1]);
 assert(add(slots,keys,86,other,slot)==AddResult::Added&&slot==1);
 // Changed live selection never changes an existing group's successor pool.
 vec(0xA8ECB8,0x210000,{unselected.pointer});put(a.pointer+0x6C,0);follow(read,slots[0]);assert(slots[0].target==b);
 // Five cells is inclusive; anything outside that radius closes the track.
 Track edge=captured;put(b.pointer+0x6C,0);follow(read,edge);assert(edge.target==c);
 put(c.pointer+0x9C,35*256+1);edge=captured;follow(read,edge);assert(!edge);
 // Reused memory cannot supply a new death position or be followed by ID.
 edge=captured;put(a.pointer+0x10,999);put(a.pointer+0x9C,99*256);put(c.pointer+0x9C,35*256);follow(read,edge);assert(edge.target==c);
 // A temporarily hidden transported/limbo focus remains the same member.
 put(a.pointer+0x10,a.id);put(a.pointer+0x6C,100);put(a.pointer+0x81,uint8_t(1));put(a.pointer+0x90,uint8_t(0));edge=captured;follow(read,edge);assert(edge.target==a);put(a.pointer+0x81,uint8_t(0));put(a.pointer+0x90,uint8_t(1));put(a.pointer+0x11C,0x40000u);follow(read,edge);assert(edge.target==a);put(a.pointer+0x11C,0u);
 // Successive deaths still use the initial group and the latest focus position.
 put(a.pointer+0x9C,30*256);put(b.pointer+0x6C,100);edge=captured;put(a.pointer+0x6C,0);follow(read,edge);assert(edge.target==b);put(b.pointer+0x9C,36*256);follow(read,edge);put(b.pointer+0x6C,0);follow(read,edge);assert(edge.target==c);
 for(int count=1;count<=3;++count)for(int height:{320,600,1080}){bool visible[3]={true,count>=2,count>=3};Box boxes[3];arrange(visible,800,height,boxes);assert(boxes[0].y>=0);int bottom=boxes[count-1].y+boxes[count-1].h;assert(std::abs(boxes[0].y-(height-bottom))<=1);for(int i=0;i<count;++i){assert(boxes[i].x==12&&boxes[i].w>0&&boxes[i].h>0&&bottom<=height);if(i)assert(boxes[i].y>=boxes[i-1].y+boxes[i-1].h+8);}}
 bool middle[3]={false,true,false};Box boxes[3];arrange(middle,800,600,boxes);assert(boxes[1].y==172&&boxes[1].w==320&&!boxes[0].w&&!boxes[2].w);
 put(c.pointer+0xA4,208);put(0xB0CD48,.144337567);int x,y;assert(camera(read,c,800,600,x,y));assert(x==int((int64_t(35*256)-30*256)*30/256)&&y==int((int64_t(35*256)+30*256)*15/256)-30);
 put(c.pointer+0x6C,0);assert(!camera(read,c,800,600,x,y));
 std::cout<<"PASS same-key allocation without borrowing, stable original cohort, nearest succession, inclusive five-cell radius, pointer reuse, transport, centered stack and live-height camera\n";
}
