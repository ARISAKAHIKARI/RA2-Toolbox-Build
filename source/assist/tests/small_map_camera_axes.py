"""Compare real native Clamp_Center with production small-map axis fixing.

The private PE is read only and executes in x86 emulation. C++ uses the exact
production header/bridge with explicit memory and patch-manager seams. No game
process is started, injected, or changed. This is not visual/netplay acceptance.
"""
from pathlib import Path
import argparse, hashlib, json, struct, subprocess
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP

HERE=Path(__file__).resolve().parent;SRC=HERE.parent/'source'
FIELDS=(0x87F8DC,0x87F8E4,0x87F8E8,0x87F8EC,0x87F8F0,0x886FA8,0x886FAC)
GEOMETRY=r'''
#include <cstdio>
#include "small_map_camera.h"
using namespace assist;
int main(){smallcamera::Geometry g;smallcamera::Point p;
 while(std::scanf("%d %d %d %d %d %d %d %d %d",&g.mapWidth,&g.x,&g.y,&g.width,&g.height,&g.viewWidth,&g.viewHeight,&p.x,&p.y)==9){auto old=p,next=p;bool a=smallcamera::clampNative(g,old),b=smallcamera::clamp(g,next);auto twice=next;bool c=smallcamera::clamp(g,twice);std::printf("%d %d %d %d %d %d %d %d %d\n",old.x,old.y,a,next.x,next.y,b,twice.x,twice.y,c);}}
'''
BRIDGE=r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <map>
#include "small_map_camera.h"
using namespace assist;
#define CHECK(c) do{++checks;if(!(c)){std::fprintf(stderr,"failed: %s at %d\n",#c,__LINE__);std::_Exit(3);}}while(0)
namespace experimentalpatch{constexpr uint32_t CameraClamp=0x6D8640;}
struct RiskSite{uint32_t address;unsigned size;uint8_t before[16],after[16];};
static struct{bool stop=false;uint32_t owner=1,smallMapClamp=0;int32_t smallMapClampStatus=0;}wire,*sh=&wire;
static bool fault=false,installed=false,foreign=false,lease=true;static int installs=0,restores=0,checks=0;
static std::map<uint32_t,int>memory;
template<class T>static T get(uint32_t p){return T(memory[p]);}
static bool restoreRiskSite(RiskSite&){++restores;if(foreign&&installed)return false;installed=false;return true;}
static bool installRiskSite(RiskSite&){++installs;if(foreign)return false;installed=true;return true;}
static int smallMapCameraAction(uint32_t);
static int GuardedRiskAction(int action,uint32_t point,uint32_t){CHECK(action==7);return lease?smallMapCameraAction(point):0;}
#include "small_map_camera_bridge.h"
static void geometry(int w,int h,int vw,int vh){int v[]={200,2,3,w,h,vw,vh};uint32_t fields[]={0x87F8DC,0x87F8E4,0x87F8E8,0x87F8EC,0x87F8F0,0x886FA8,0x886FAC};for(int i=0;i<7;++i)memory[fields[i]]=v[i];}
int main(){
 geometry(20,40,1920,900);smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==0&&installs==0);
 sh->smallMapClamp=1;smallMapCameraTick();CHECK(installed&&sh->smallMapClampStatus==1);
 smallcamera::Point p{100000,100000};CHECK(smallMapClamp(&p));auto center=smallcamera::center(cameraGeometry());CHECK(p.x==center.x&&p.y==smallcamera::limits(cameraGeometry()).bottom);
 geometry(40,10,800,1080);smallMapCameraTick();p={-100000,-100000};CHECK(smallMapClamp(&p));center=smallcamera::center(cameraGeometry());CHECK(p.y==center.y&&p.x==smallcamera::limits(cameraGeometry()).left);
 geometry(20,10,1921,1081);smallMapCameraTick();p={100000,100000};CHECK(smallMapClamp(&p));center=smallcamera::center(cameraGeometry());CHECK(p.x==center.x&&p.y==center.y);CHECK(!smallMapClamp(&p));
 // Zoom/sidebar changes the live viewport; crossing either threshold must
 // restore or re-enable the same owned site without touching another owner.
 geometry(20,40,1152,900);smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==2);
 geometry(20,40,1280,900);smallMapCameraTick();CHECK(installed&&sh->smallMapClampStatus==1);
 geometry(90,80,1920,1080);smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==2);
 geometry(20,40,0,900);smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==3);
 geometry(20,40,1920,900);foreign=true;smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==-2);foreign=false;smallMapCameraTick();CHECK(installed);
 // A denied Capture keeps native semantics; no new writer may bypass it.
 lease=false;p={100000,100000};auto original=p;smallcamera::clampNative(cameraGeometry(),original);smallMapClamp(&p);CHECK(p.x==original.x&&p.y==original.y);lease=true;
 sh->smallMapClamp=0;smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==0);
 sh->smallMapClamp=1;smallMapCameraTick();sh->stop=true;smallMapCameraTick();CHECK(!installed&&sh->smallMapClampStatus==0);
 sh->stop=false;smallMapCameraTick();foreign=true;sh->smallMapClamp=0;smallMapCameraTick();CHECK(installed&&sh->smallMapClampStatus==-2);
 std::printf("{\"checks\":%d,\"assertions_active\":true,\"pass\":true}\n",checks);
}
'''

def native_fixture(path):
    pe=pefile.PE(str(path));u=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
    body=pe.get_data(0x6D8640-base,0xBE);assert body[:5]==bytes.fromhex('A1 E4 F8 87 00')
    fnv=2166136261
    for b in body[5:]:fnv=((fnv^b)*16777619)&0xffffffff
    assert fnv==0xA41008F5,'native clamp ABI/body differs from managed patch context'
    u.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095);u.mem_write(base,pe.get_memory_mapped_image())
    point,stack,stop=0x20000000,0x21000000,0x22000000
    for at in (point,stack,stop):u.mem_map(at,4096)
    def invoke(g,p):
        for at,v in zip(FIELDS,g):u.mem_write(at,struct.pack('<i',v))
        u.mem_write(point,struct.pack('<2i',*p));sp=stack+2048;u.mem_write(sp,struct.pack('<2I',stop,point));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,0x12345678)
        regs={UC_X86_REG_EBX:0x31415926,UC_X86_REG_ESI:0x27182818,UC_X86_REG_EDI:0x13579BDF,UC_X86_REG_EBP:0x2468ACE0}
        for reg,v in regs.items():u.reg_write(reg,v)
        u.emu_start(0x6D8640,stop,count=1000)
        assert u.reg_read(UC_X86_REG_EIP)==stop and u.reg_read(UC_X86_REG_ESP)==sp+8
        assert all(u.reg_read(reg)==v for reg,v in regs.items())
        return (*struct.unpack('<2i',u.mem_read(point,8)),u.reg_read(UC_X86_REG_EAX)&255)
    return invoke

def cases():
    out=[]
    for width,height in ((20,40),(40,10),(20,10),(90,80),(12,8)):
        for vw,vh in ((800,600),(1920,1080),(1921,1081),(1152,900),(1280,900),(2392,1408),(2560,1408),(1200,1335)):
            g=(200,2,3,width,height,vw,vh);ml=30*(2*g[1]-g[0]);mt=15*(2*g[2]+g[0]-5)
            lo=(vw//2+ml,vh//2+mt);hi=(lo[0]+60*width-vw,lo[1]+30*height+135-vh)
            xs=(-100000,lo[0]-1,lo[0],(lo[0]+hi[0])//2,hi[0],hi[0]+1,100000)
            ys=(-100000,lo[1]-1,lo[1],(lo[1]+hi[1])//2,hi[1],hi[1]+1,100000)
            for x in xs:
                for y in ys:out.append((g,(x,y)))
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True,type=Path);p.add_argument('--game',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    game_hash=hashlib.sha256(a.game.read_bytes()).hexdigest();rows=cases();invoke=native_fixture(a.game);results={}
    for arch in ('x86_64','x86'):
        cpp=a.out/('geometry-'+arch+'.cpp');exe=a.out/('geometry-'+arch+'.exe');cpp.write_text(GEOMETRY,encoding='utf-8')
        subprocess.run([str(a.zig),'c++','-std=c++17','-O2','-UNDEBUG','-target',arch+'-windows-gnu','-I'+str(SRC),str(cpp),'-o',str(exe)],check=True)
        run=subprocess.run([str(exe)],input='\n'.join(' '.join(map(str,(*g,*q)))for g,q in rows)+'\n',capture_output=True,text=True,check=True)
        outputs=[tuple(map(int,l.split()))for l in run.stdout.splitlines()];assert len(outputs)==len(rows)
        fixed_x=fixed_y=normal=odd=0
        for (g,q),value in zip(rows,outputs):
            native=invoke(g,q);assert value[:3]==native,(g,q,value,native)
            mw,mh=g[3]*60,g[4]*30+135;degx=mw<g[5];degy=mh<g[6]
            center=(30*(2*g[1]-g[0])+mw//2,15*(2*g[2]+g[0]-5)+mh//2)
            expected=(center[0]if degx else native[0],center[1]if degy else native[1])
            assert value[3:5]==expected and value[5]==int(expected!=q),(g,q,value,expected)
            assert value[6:]==(*expected,0),'not idempotent'
            fixed_x+=degx and not degy;fixed_y+=degy and not degx;normal+=not degx and not degy;odd+=bool((g[5]|g[6])&1)
        results[arch]={'native_comparison_cases':len(rows),'horizontal_only':fixed_x,'vertical_only':fixed_y,'normal_large_or_equal':normal,'odd_viewport':odd,'pass':True}
    cpp=a.out/'bridge.cpp';exe=a.out/'bridge-x86.exe';cpp.write_text(BRIDGE,encoding='utf-8')
    subprocess.run([str(a.zig),'c++','-std=c++17','-O2','-UNDEBUG','-target','x86-windows-gnu','-I'+str(SRC),str(cpp),'-o',str(exe)],check=True)
    run=subprocess.run([str(exe)],capture_output=True,text=True,check=True,timeout=20);results['production_bridge']=json.loads(run.stdout)
    assert hashlib.sha256(a.game.read_bytes()).hexdigest()==game_hash
    report={'scope':__doc__,'game_sha256':game_hash,'game_hash_stable':True,'native_entry':'0x6D8640','native_body_bytes':190,'native_abi_context_fnv_without_entry':'A41008F5','architectures':results,'source_sha256':{n:hashlib.sha256((SRC/n).read_bytes()).hexdigest()for n in ('small_map_camera.h','small_map_camera_bridge.h')}}
    (a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))

if __name__=='__main__':main()
