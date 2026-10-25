"""Exercise passenger/garrison tracking through the production watch renderer.
Constructed memory and boundary asset/GDI doubles are not a real-game test.
"""
from pathlib import Path
import subprocess,tempfile

root=Path(__file__).resolve().parents[2]
prefix=(root/'assist/tests/watch_host.cpp').read_text().split('int main(){',1)[0]
scenario=r'''
int main(){
 auto put=[](uint32_t p,auto v){memcpy(memory.data()+p,&v,sizeof v);};
 auto vec=[&](uint32_t p,uint32_t storage,const std::vector<uint32_t>&items){put(p+4,storage);put(p+8,int(items.size()));put(p+16,int(items.size()));memcpy(memory.data()+storage,items.data(),items.size()*4);};
 constexpr uint32_t infantry=0x10000,carrier=0x12000,nearby=0x14000,building=0x16000,cells=0x200000;
 auto make=[&](uint32_t p,uint32_t vt,uint32_t id,int x,int y){uint32_t type=0x800000+p;put(p,vt);put(p+0x10,id);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,x*256+128);put(p+0xA0,y*256+128);put(p+0x21C,0x180000u);put(p+(vt==0x7EB058?0x6C0:vt==0x7E3EBC?0x520:0x6C4),type);put(type+0xB0,1u);put(type+0xCA1,uint8_t(1));};
 make(infantry,0x7EB058,11,30,30);make(carrier,0x7F5C70,22,32,30);make(nearby,0x7F5C70,33,30,31);make(building,0x7E3EBC,44,35,35);
 vec(0xA8EC78,0x190000,{infantry,carrier,nearby,building});vec(0xA8EB40,0x195000,{building});
 put(external::radar::Map+0xF4,32);put(external::radar::Map+0xF8,64);put(external::radar::Map+0x104,32);put(external::radar::Map+0x108,64);
 put(external::radar::Map+0x13C,cells);put(external::radar::Map+0x140,512*512);put(0xA8ED84,1);
 uint32_t cell=0x400000;for(int y=10;y<=50;++y)for(int x=10;x<=50;++x){put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x120,int8_t(-1));put(cell+0x121,int8_t(-1));put(cell+0x12C,uint8_t(0x10));put(cell+0x140,uint8_t(1));cell+=0x150;}
 external::watch::Target tracked{infantry,11};engine.process=pid;engine.epoch=1;engine.tracks[0].target=tracked;engine.tracks[0].members={tracked};
 engine.update(clockValue);assert(engine.views[0].frame.focus==tracked&&engine.views[0].frame.center.target==tracked);
 auto assertUpdated=[&](external::watch::Target spatial){auto&view=engine.views[0];assert(engine.tracks[0].target==tracked&&engine.tracks[0].members.size()==1&&view.shown&&view.tick==clockValue&&view.frame.focus==tracked&&view.frame.center.target==spatial);
  auto found=std::find_if(view.frame.actors.begin(),view.frame.actors.end(),[&](const auto&a){return a.target.pointer==nearby;});assert(found!=view.frame.actors.end());int x=0;memcpy(&x,memory.data()+nearby+0x9C,4);assert(found->x==x);
 };
 put(infantry+0x81,uint8_t(1));put(infantry+0x90,uint8_t(0));put(infantry+0x11C,carrier);put(carrier+0x114,1);put(carrier+0x118,infantry);
 for(int n=0;n<30;++n){put(carrier+0x9C,32*256+128+n*24);put(nearby+0x9C,30*256+128+n*4);engine.update(clockValue+=2);assertUpdated({carrier,22});assert(engine.views[0].frame.center.x==32*256+128+n*24);assert(std::none_of(engine.views[0].frame.actors.begin(),engine.views[0].frame.actors.end(),[&](const auto&a){return a.target==tracked;}));}
 auto fresh=[](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);};int cameraX=0,cameraY=0;assert(external::watch::camera(fresh,tracked,800,600,cameraX,cameraY));assert(cameraX==int((int64_t(32*256+128+29*24)-(30*256+128))*30/256));
 put(infantry+0x11C,0u);put(infantry+0x81,uint8_t(0));put(infantry+0x90,uint8_t(1));put(infantry+0x9C,34*256+128);put(carrier+0x114,0);put(carrier+0x118,0u);engine.update(clockValue+=2);assertUpdated(tracked);
 // Occupants membership, rather than a destination or nearby-building guess,
 // anchors garrisoned infantry. The original identity/group stays pinned.
 put(infantry+0x81,uint8_t(1));put(infantry+0x90,uint8_t(0));vec(building+0x684,0x198000,{infantry});engine.update(clockValue+=2);assertUpdated({building,44});assert(engine.views[0].frame.center.x==35*256+128);
 for(int n=0;n<20;++n){put(nearby+0x9C,31*256+128+n*4);engine.update(clockValue+=2);assertUpdated({building,44});}
 // An unreadable/missing association keeps only the last camera position;
 // nearby actor coordinates and scene revisions still advance every tick.
 blockedAddress=building+0x688;put(nearby+0x9C,33*256+128);engine.update(clockValue+=2);assertUpdated({});assert(engine.views[0].frame.center.x==35*256+128);blockedAddress=0;
 vec(building+0x684,0x198000,{});put(nearby+0x9C,33*256+132);engine.update(clockValue+=2);assertUpdated({});assert(engine.views[0].frame.center.x==35*256+128);
 auto discoveries=occupantDiscoveryReads;for(int n=0;n<25;++n){put(nearby+0x9C,33*256+136+n*4);engine.update(clockValue+=2);assertUpdated({});}assert(occupantDiscoveryReads==discoveries);
 put(0xA8ED84,2);engine.update(clockValue+=2);assertUpdated({});assert(occupantDiscoveryReads==discoveries+1&&"unknown-host discovery must be shared between lifecycle and scene sampling");
 // A thousand empty buildings do not create a thousand uncached life scans.
 // Inspect at most 64 headers per new frame and resume the cursor later.
 std::vector<uint32_t> manyBuildings;for(int n=0;n<1000;++n)manyBuildings.push_back(0x600000+n*0x800);manyBuildings[70]=building;vec(building+0x684,0x198000,{infantry});vec(0xA8EB40,0x1A0000,manyBuildings);
 auto prior=engine.views[0].frame.center;prior.searched=false;prior.discoveryCursor=0;int headers=0;
 auto budgetRead=[&](uint32_t p,void*out,size_t n){if(n==16&&p>=0x600688&&p<0x800000)++headers;return externalReader.readFresh(p,out,n);};
 put(0xA8ED84,3);auto incremental=external::watch::anchor(budgetRead,tracked,prior);assert(incremental.located&&!incremental.target&&incremental.discoveryCursor==64&&headers==64);
 headers=0;for(int n=0;n<20;++n)incremental=external::watch::anchor(budgetRead,tracked,incremental);assert(headers==0);
 put(0xA8ED84,4);incremental=external::watch::anchor(budgetRead,tracked,incremental);assert(incremental.target.pointer==building&&headers<=64);vec(0xA8EB40,0x195000,{building});vec(building+0x684,0x198000,{});
 put(infantry+0x81,uint8_t(0));put(infantry+0x90,uint8_t(1));put(infantry+0x9C,36*256+128);engine.update(clockValue+=2);assertUpdated(tracked);
 // A renderer cache with a stale zero HP cannot permanently remove a live
 // manual track: lifecycle decisions use independent uncached observations.
 staleLifeAddress=infantry+0x6C;engine.update(clockValue+=2);assert(engine.tracks[0].target==tracked&&engine.views[0].shown);staleLifeAddress=0;engine.update(clockValue+=2);assertUpdated(tracked);
 put(infantry+0x6C,0);engine.update(clockValue+=2);assert(!engine.tracks[0]&&!engine.views[0].shown);
 std::cout<<"PASS passenger and garrison entry/exit, moving carrier and unchanged simulation-frame scene refresh, verified host membership, last-center scene refresh, original identity/cohort, fresh lifecycle and actual death\n";
}
'''

if __name__=='__main__':
 with tempfile.TemporaryDirectory() as tmp:
  cpp=Path(tmp)/'watch_passenger.cpp';exe=Path(tmp)/'watch_passenger';cpp.write_text(prefix+scenario)
  subprocess.run(['g++','-std=c++17','-O1','-pthread','-I'+str(root/'assist/tests'),str(cpp),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True,timeout=20)
