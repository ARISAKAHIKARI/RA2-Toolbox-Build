"""Execute the shared tracking-camera compositor with synthetic memory/assets.
Uses the production SceneRenderer/source bank; tests GPU setting propagation,
not a Windows display or a physical GPU driver.
"""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
prefix=(root/'assist/tests/watch_host.cpp').read_text().split('int main(){',1)[0]
prefix=prefix.replace('struct Backend {','''#include "../../external_reader/source/render_profile.h"
struct Backend {
 bool requestedGPU=false,voxelLight=true,spriteLight=true;int detail=0,precision=0,resolutions=0;
 struct Files {bool load(const char*,std::vector<uint8_t>&){return false;}}files;
 void presentationGPU(bool value){requestedGPU=value;}bool presentationGPU()const{return false;}
 void presentationDetail(int value){detail=value;}void presentationColorPrecision(bool value){precision=value;}
 void presentationPalette(external::pixelupgrade::PaletteRange){}void presentationResolution(int,int,int,int){++resolutions;}
 void presentationLighting(bool v,bool s){voxelLight=v;spriteLight=s;}
''')
prefix=prefix.replace('return gameassets::palette(b,p);','p.fill(0xffffffff);return true;')
prefix+='\n#include "../source/watch_scene.h"\n'
scenario=r'''
int main(){
 auto put=[](uint32_t p,auto value){memcpy(memory.data()+p,&value,sizeof value);};
 put(0xA8E378,uint8_t(1));put(0xA8E9A0,uint8_t(1));put(0xA8ED84,100);put(0xA83D4C,0x180000u);
 constexpr uint32_t cells=0x200000;put(external::radar::Map+0x13C,cells);put(external::radar::Map+0x140,512*512);
 put(external::radar::Map+0xF4,64);put(external::radar::Map+0xF8,128);put(external::radar::Map+0x104,64);put(external::radar::Map+0x108,128);
 put(external::radar::Map+0x1240,256);put(external::radar::Map+0x1244,256);put(external::radar::Map+0x1490,128);put(external::radar::Map+0x1498,128);put(external::radar::Map+0x1488,1.f);put(external::radar::Map+0x14A4,256);put(external::radar::Map+0x14A8,256);
 uint32_t object=0x10000,type=0x800000;put(object,0x7F5C70u);put(object+0x10,2u);put(object+0x6C,100);put(object+0x74,uint8_t(1));put(object+0x90,uint8_t(1));put(object+0x6C4,type);put(object+0x21C,0x180000u);put(object+0x9C,64*256+128);put(object+0xA0,64*256+128);
 auto terrain=std::make_shared<external::radar::Terrain>();terrain->geo={64,128,128,128,256,256,1,cells,0x180000,512*512,{64,128,0,0,64,128},256,256};terrain->reveal=true;terrain->art.resize(512*512);terrain->colors.resize(512*512);terrain->art[64*512+64].type=1;terrain->colors[64*512+64]=0xabcdef;
 external::radar::Frame data;data.terrain=terrain;data.frame=100;data.tick=clockValue;data.markerScanValid=true;data.posesPrepared=true;
 external::radar::Marker marker;marker.object=object;marker.id=2;marker.type=type;marker.owner=0x180000;marker.kind=2;marker.x=marker.y=64*256+128;data.markers.push_back(marker);
 auto source=std::make_shared<const external::radar::Frame>(data);external::radar::sceneBank().publish(pid,1,true,source);
 watchscene::Renderer<decltype(externalReader),Backend> camera(externalReader,backend);camera.process=pid;camera.epoch=1;camera.revealMap=true;camera.autoSuppressed=true;camera.tracks[0].target={object,2};
 external::presentation::Profile profile;profile.gpu=true;profile.detail=1;profile.precision=1;profile.voxelLighting=false;profile.spriteLighting=false;camera.features(profile);
 // No world-object vector exists in memory. Reaching a complete scene proves
 // that the preferred path consumed the bank instead of resampling actors.
 for(int i=0;i<80&&!camera.views[0].scene;++i)assert(camera.update(clockValue));
 assert(camera.sharedSource&&camera.sourceTick==source->tick&&camera.views[0].scene&&camera.views[0].scene->pixels.size()==640*512);
 assert(camera.views[0].frame.focus.pointer==object&&backend.requestedGPU&&!backend.voxelLight&&!backend.spriteLight&&backend.detail==1&&backend.precision==1&&backend.resolutions>0);
 // The source stamp is never renewed by rendering, even while paused.
 assert(camera.update(clockValue+99)&&camera.sourceTick==source->tick);assert(!camera.update(clockValue+501));
 // A different house/match cannot reuse the cached source or its image.
 put(0xA83D4C,0x190000u);assert(!camera.update(clockValue));camera.reset();assert(!camera.views[0].scene);
 std::cout<<"PASS shared watch scene, no actor-vector resample, source age, world separation and GPU/lighting/detail propagation\n";
}
'''
with tempfile.TemporaryDirectory(dir=root/'assist/tests') as folder:
 cpp=Path(folder)/'watch-scene.cpp';exe=Path(folder)/'watch-scene';cpp.write_text(prefix+scenario)
 subprocess.run(['g++','-std=c++17','-O2','-UNDEBUG','-pthread','-I'+str(root/'assist/tests'),str(cpp),str(root/'third_party/xbrz/xbrz.cpp'),'-ldl','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
