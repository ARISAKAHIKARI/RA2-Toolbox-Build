"""Run production watch/minimap samplers and the native watch compositor.
Memory, decoded actor art and GDI are boundary doubles; WRENCH.SHP decoding,
palette conversion and the final no-Z repair pass use production code.
"""
from pathlib import Path
import subprocess,tempfile

root=Path(__file__).resolve().parents[2]
prefix=(root/'assist/tests/watch_host.cpp').read_text().split('int main(){',1)[0]
scenario=r'''
struct EffectsAssets:Backend {
 struct Read {bool operator()(uint32_t p,void*out,size_t n)const{return externalReader.readFresh(p,out,n);}} read;
 struct Files {bool load(const char*,std::vector<uint8_t>&){return false;}} files;
 int projectileCaptures=0;
 bool captureProjectile(uint32_t,uint32_t,uint32_t id,ActorArt&art){++projectileCaptures;art.marker=int(id)+1000;return true;}
 bool renderActor(const ActorArt&art,const external::visual::Pose&pose,external::voxel::Image&image){
  image={};image.width=image.height=art.marker>=1000?3:64;image.x=image.y=art.marker>=1000?-1:-32;
  image.pixels.assign(image.width*image.height,art.marker>=1000?(pose.frame==2?0xff20ff40:0xffaa20ff):0xff204080);
  image.depth.assign(image.pixels.size(),art.marker>=1000?0.f:100000.f);image.writeDepth=art.marker<1000;return true;
 }
};
int main(){
 auto put=[](uint32_t p,auto value){memcpy(memory.data()+p,&value,sizeof value);};
 auto vec=[&](uint32_t p,uint32_t storage,const std::vector<uint32_t>&v){put(p+4,storage);put(p+8,int(v.size()));put(p+16,int(v.size()));memcpy(memory.data()+storage,v.data(),v.size()*4);};
 constexpr uint32_t focus=0x10000,building=0x12000,bullet=0x14000,bulletType=0x860000,convert=0x50000,shape=0x60000,palette=0x70000,cells=0x200000;
 auto techno=[&](uint32_t p,uint32_t vt,uint32_t id,int x,int y){put(p,vt);put(p+0x10,id);put(p+0x6C,100);put(p+0x74,uint8_t(1));put(p+0x90,uint8_t(1));put(p+0x9C,x*256+128);put(p+0xA0,y*256+128);put(p+0x21C,0x180000u);put(p+(vt==0x7E3EBC?0x520:0x6C4),0x800000u+p);};
 techno(focus,0x7F5C70,41,64,64);techno(building,0x7E3EBC,42,65,64);vec(0xA8EC78,0x190000,{focus,building});
 put(external::radar::Map+0xF4,64);put(external::radar::Map+0xF8,64);put(external::radar::Map+0x104,64);put(external::radar::Map+0x108,64);put(external::radar::Map+0x13C,cells);put(external::radar::Map+0x140,512*512);
 put(external::radar::Map+0x1488,1.f);put(external::radar::Map+0x1490,64);put(external::radar::Map+0x1498,64);put(external::radar::Map+0x14A4,128);put(external::radar::Map+0x14A8,128);put(external::radar::Map+0x1240,128);put(external::radar::Map+0x1244,128);
 put(0xA83D4C,0x180000u);put(0x180000+22265,uint32_t(0x004020d0));put(0xA8E9A0,uint8_t(1));put(0xA8E378,uint8_t(1));put(0xA8ED84,100);put(0xA8EB60,1);
 uint32_t cell=0x400000;for(int y=40;y<=90;++y)for(int x=40;x<=90;++x){put(cells+(y*512+x)*4,cell);put(cell+0x24,int16_t(x));put(cell+0x26,int16_t(y));put(cell+0x38,-1);put(cell+0x44,-1);put(cell+0x120,int8_t(-1));put(cell+0x121,int8_t(-1));cell+=0x150;}
 // A real memory SHP with seven poses and a one-shade cursor converter.
 put(0x89DDC8,shape);put(0x87F6C8,convert);put(convert,0x7E5358u);put(convert+4,2);put(convert+0x16C,1);put(convert+0x174,palette);
 for(int i=1;i<256;++i)put(palette+i*2,uint16_t(0xf800));
 put(shape+2,uint16_t(4));put(shape+4,uint16_t(6));put(shape+6,uint16_t(7));
 for(int i=0;i<7;++i){uint32_t f=shape+8+i*24,p=8+7*24+i*4;put(f,uint16_t(1));put(f+2,uint16_t(2));put(f+4,uint16_t(2));put(f+6,uint16_t(2));put(f+20,p);put(shape+p,uint32_t(0x01000001));}
 put(building+0x6E8,uint8_t(1));
 // Zero-damage bullets remain visible. Their state/frames use the independent
 // projectile reader, never Techno health or unit-facing logic.
 put(bullet,external::projectiles::Vtable);put(bullet+0x10,91u);put(bullet+0xAC,bulletType);put(bulletType,external::projectiles::TypeVtable);put(bullet+0x90,uint8_t(1));put(bullet+0x94,2);put(bullet+0x9C,62*256+128);put(bullet+0xA0,64*256+128);put(bulletType+0x2A1,uint8_t(1));put(bulletType+0x2F4,uint8_t(1));put(bullet+0x12C,uint8_t(2));put(0x87F6C4,convert);vec(external::projectiles::Array,0x1B0000,{bullet});
 EffectsAssets assets;watchengine::Renderer<decltype(externalReader),EffectsAssets> renderer(externalReader,assets);renderer.process=pid;renderer.epoch=1;renderer.tracks[0].target={focus,41};renderer.tracks[0].members={{focus,41}};
 renderer.update(clockValue);auto&view=renderer.views[0];assert(view.shown&&view.frame.repairs.size()==1);
 auto projectile=std::find_if(view.frame.actors.begin(),view.frame.actors.end(),[](const auto&a){return a.kind==external::projectiles::Kind;});assert(projectile!=view.frame.actors.end()&&projectile->target.id==91&&projectile->pose.frame==2&&assets.projectileCaptures==1);
 Graphics g;renderer.drawView(g,view);assert(view.raster.width()==640&&view.raster.height()==512);
 assert(std::find(view.raster.color.begin(),view.raster.color.end(),0xff20ff40)!=view.raster.color.end());
 assert(view.raster.color[255*640+349]==0xffff0000&&view.raster.depth[255*640+349]>99990.f); // final wrench ignores the roof depth
 // Both sampling paths collect the same actual repair/projectile instances.
 auto read=assets.read;external::radar::Sampler sampler(read);for(int i=0;i<16;++i)assert(sampler.step(1,16384,true));auto radar=sampler.snapshot(clockValue);
 assert(radar.terrain&&radar.repairs.size()==1&&std::any_of(radar.markers.begin(),radar.markers.end(),[](const auto&m){return m.kind==external::projectiles::Kind&&m.id==91&&m.pose.frame==2;}));
 // Same simulation frame: changed repair state and projectile animation still
 // refresh; the source art stays shared rather than being recaptured.
 put(building+0x6E8,uint8_t(0));put(bullet+0x12C,uint8_t(3));renderer.update(clockValue+=2);assert(view.frame.repairs.empty());renderer.drawView(g,view);
 assert(view.raster.color[255*640+349]!=0xffff0000&&std::find(view.raster.color.begin(),view.raster.color.end(),0xffaa20ff)!=view.raster.color.end()&&assets.projectileCaptures==1);
 put(bulletType+0x29E,uint8_t(1));renderer.update(clockValue+=2);assert(std::none_of(view.frame.actors.begin(),view.frame.actors.end(),[](const auto&a){return a.kind==external::projectiles::Kind;}));
 std::cout<<"PASS production watch/minimap effect collection, native 640x512 composition, zero-damage projectile poses, same-frame updates, shared art, real wrench decode/palette, final no-Z pass and native invisible filtering\n";
}
'''
with tempfile.TemporaryDirectory() as tmp:
 cpp=Path(tmp)/'watch_effects.cpp';exe=Path(tmp)/'watch_effects';cpp.write_text(prefix+scenario)
 subprocess.run(['g++','-std=c++17','-O1','-pthread','-I'+str(root/'assist/tests'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=30)
