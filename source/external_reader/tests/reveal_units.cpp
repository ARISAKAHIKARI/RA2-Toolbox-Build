#include "../source/reveal_scene.h"
#include "../../suite/source/display_settings.h"
#include <cassert>
#include <iostream>
#include <random>
#include <unordered_map>
using namespace external;
struct Memory {
 std::unordered_map<uint32_t,uint8_t> bytes;
 template<class T>void put(uint32_t p,T value){const auto*b=reinterpret_cast<const uint8_t*>(&value);for(unsigned n=0;n<sizeof(T);++n)bytes[p+n]=b[n];}
 bool operator()(uint32_t p,void*out,size_t n)const{auto*b=static_cast<uint8_t*>(out);for(size_t k=0;k<n;++k){auto f=bytes.find(p+k);b[k]=f==bytes.end()?0:f->second;}return true;}
};
static unsigned cases=0;
void policy(){
 for(int kind=-1;kind<=4;++kind)for(int cloak=0;cloak<=3;++cloak)for(unsigned flags=0;flags<16;++flags)for(unsigned opt=0;opt<8;++opt){
  bool permanent=flags&1,visible=flags&2,disguised=flags&4,enemy=flags&8;
  auto d=reveal::decide(opt,{kind,cloak,permanent,visible,disguised,enemy});
  bool valid=kind>=0&&kind<=2&&!permanent;
  assert(d.uncloak==bool(valid&&(opt&1)&&!visible&&cloak));
  assert(d.trueForm==bool(valid&&(visible||d.uncloak)&&disguised&&((kind==2&&(opt&2))||(kind==1&&enemy&&(opt&4)))));
  ++cases;
 }
 // Real capture: a hidden enemy unit becomes drawable only through presentation.
 Memory memory;constexpr uint32_t p=0x10000,type=0x20000,house=0x30000,owner=0x40000;
 memory.put<uint32_t>(p,0x7F5C70);memory.put<uint32_t>(p+0x21C,owner);memory.put<int>(p+0x220,2);
 memory.put<int>(house+0x30,0);memory.put<uint32_t>(0xB73550,0x123400);memory.put<uint8_t>(p+0x74,1);memory.put<uint8_t>(p+0x90,1);memory.put<int>(p+0x6C,100);
 auto read=[&](uint32_t a,void*b,size_t n){return memory(a,b,n);};GameView view(read);
 assert(nativedraw::alive(view,p,2));
 assert(!nativedraw::capture(read,p,type,2,house,100).visible);
 auto shown=nativedraw::capture(read,p,type,2,house,100,{true,false});
 assert(shown.visible&&shown.type==type&&shown.owner==owner&&shown.material.translucency==0);
 assert(!nativedraw::capture(read,p,type,2,house,100).visible); // no write back
 memory.put<uint8_t>(type+0xC9A,1);assert(!nativedraw::capture(read,p,type,2,house,100,{true,true}).visible);
 memory.put<uint8_t>(p+0x81,1);assert(!nativedraw::alive(view,p,2));memory.put<uint8_t>(p+0x81,0);
 memory.put<int>(p+0x6C,0);assert(!nativedraw::alive(view,p,2));memory.put<int>(p+0x6C,100);
 memory.put<uint8_t>(p+0x74,0);assert(!nativedraw::alive(view,p,2));cases+=8;
 memory.put<uint8_t>(p+0x74,1);memory.put<uint8_t>(type+0xC9A,0);memory.put<int>(p+0x220,0);
 constexpr uint32_t cell=0x50000,items=0x60000,disguise=0x70000,disguiseOwner=0x80000;
 memory.put<uint32_t>(0x87F7E8+0x13C,items);memory.put<int>(0x87F7E8+0x140,1);memory.put<uint32_t>(items,cell);
 memory.put<uint8_t>(p+0x1D8,1);memory.put<uint32_t>(p+0x518,disguise);memory.put<uint32_t>(p+0x51C,disguiseOwner);memory.put<uint32_t>(disguise,0x7EB610);
 memory.put<uint32_t>(p,0x7EB058);memory.put<int>(owner+0x30,2);
 auto spy=nativedraw::capture(read,p,type,1,house,100);assert(spy.disguised&&spy.type==disguise&&spy.owner==disguiseOwner);
 auto realSpy=nativedraw::capture(read,p,type,1,house,100,{false,true});assert(!realSpy.disguised&&realSpy.type==type&&realSpy.owner==owner);
 assert(view.enemy(p,house));memory.put<uint32_t>(house+0x5788,1u<<2);assert(!view.enemy(p,house));memory.put<uint32_t>(house+0x5788,0);
 memory.put<uint32_t>(p+0x21C,house);assert(!view.enemy(p,house));memory.put<uint32_t>(p+0x21C,owner);
 memory.put<int>(p+0x220,2);assert(!nativedraw::capture(read,p,type,1,house,100,{false,true}).visible);
 realSpy=nativedraw::capture(read,p,type,1,house,100,{true,true});assert(realSpy.visible&&realSpy.type==type&&realSpy.owner==owner);
 // Mirage uses the TerrainType catalogue, while true-form capture keeps UnitType.
 memory.put<int>(p+0x220,0);memory.put<uint32_t>(p,0x7F5C70);memory.put<uint32_t>(disguise,0x7F5180);
 memory.put<uint32_t>(0xA8E318+4,items+4);memory.put<int>(0xA8E318+8,1);memory.put<int>(0xA8E318+16,1);memory.put<uint32_t>(items+4,disguise);
 auto mirage=nativedraw::capture(read,p,type,2,house,100);assert(mirage.disguised&&mirage.kind==4);
 auto tank=nativedraw::capture(read,p,type,2,house,100,{false,true});assert(!tank.disguised&&tank.kind==2&&tank.type==type);
 cases+=9;
}
void alternatives(){
 radar::Frame f;radar::Marker native,truth;
 native.object=0x10000;native.id=12;native.type=0x20000;native.kind=4;native.owner=1;native.displayOrder=9;
 truth=native;truth.kind=2;truth.type=0x30000;truth.owner=2;truth.disguised=false;
 f.markers.push_back(native);f.revealed.push_back({native,truth,true});auto changed=reveal::apply(f);
 assert(f.markers[0].kind==4&&changed.markers.size()==1&&changed.markers[0].kind==2&&changed.markers[0].owner==2&&changed.markers[0].displayOrder==9);
 auto hidden=truth;hidden.object+=4;hidden.id=13;f.revealed.push_back({hidden,hidden,false});changed=reveal::apply(f);assert(changed.markers.size()==2);
 assert(reveal::stillPresent(f,{reveal::identity(truth)}));f.revealed[0].shown.id++;assert(!reveal::stillPresent(f,{reveal::identity(truth)}));
 // Reused pointer is a different unit, never a stale disguise replacement.
 assert(reveal::apply(f).markers[0].type==native.type);assert(!reveal::stillPresent(f,{}));cases+=6;
}
void masks(){
 std::mt19937 rng(235119);constexpr int w=64,h=48;
 for(int trial=0;trial<200;++trial){std::vector<overlaycomposition::Rect> keep,holes;
  for(int n=0;n<20;++n){auto&list=n<15?keep:holes;list.push_back({int(rng()%100)-20,int(rng()%80)-20,int(rng()%40),int(rng()%30)});}
  auto spans=sparsepatch::mask(w,h,keep,holes);assert(spans);
  std::vector<bool> covered(w*h,false);for(auto r:*spans)for(int x=r.x;x<r.x+r.w;++x)covered[r.y*w+x]=true;
  auto inside=[](auto r,int x,int y){return r.w>0&&r.h>0&&x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;};
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){bool expected=true;for(auto r:keep)if(inside(r,x,y))expected=false;for(auto r:holes)if(inside(r,x,y))expected=true;assert(covered[y*w+x]==expected);}
  ++cases;
 }
 assert(sparsepatch::mask(10,10,{})->size()==10);assert(sparsepatch::mask(10,10,{{0,0,10,10}})->empty());
 assert(!sparsepatch::mask(8193,10,{}));assert(!sparsepatch::mask(8192,8192,{}));
 assert(!sparsepatch::mask(4096,64,std::vector<overlaycomposition::Rect>(2049,{0,0,1,1})));
 std::vector<overlaycomposition::Rect> stripes;for(int x=0;x<1024;x+=2)stripes.push_back({x,0,1,100});assert(!sparsepatch::mask(1024,100,stripes));cases+=6;
}
void bounds(){
 mainview::View view;view.width=100;view.height=80;radar::Marker m;m.kind=2;
 voxel::Image tank;tank.width=10;tank.height=8;tank.x=-5;tank.y=-4;
 std::vector<overlaycomposition::Rect> rects;assert(reveal::rectangles(tank,m,view,150,120,.5,rects));assert(rects.size()==1&&rects[0].w==11&&rects[0].h==9);
 voxel::Image tree;tree.width=60;tree.height=70;tree.x=-30;tree.y=-60;
 m.x=m.y=256;assert(reveal::rectangles(tree,m,view,150,120,.5,rects));assert(rects.back().w>rects.front().w);
 tree.attachments.push_back(tank);tree.attachments[0].shadow=true;m.pose.shadow.known=true;m.pose.shadow.drop=20;
 assert(reveal::rectangles(tree,m,view,150,120,.5,rects));assert(rects.size()==4);
 tank.incomplete=true;assert(!reveal::rectangles(tank,m,view,150,120,.5,rects));cases+=4;
}
int main(){policy();alternatives();masks();bounds();
 for(auto option:{suite::RevealCloakedUnits,suite::RevealMirageUnits,suite::RevealEnemySpies}){assert(suite::validDisplayEdit(option,0)&&suite::validDisplayEdit(option,1)&&!suite::validDisplayEdit(option,2));++cases;}
 std::cout<<"{\"passed\":true,\"controlled_cases\":"<<cases<<",\"live_game_tested\":false}\n";
}
