"""Independent production capture/decoder/cache pixels for a held building slot.

Reuses only the public host fixture's file/time/memory boundaries and helpers.
Its unrelated overlay/terrain/voxel assertions are not executed. The 24-frame
SHP and palette are synthetic; no game process, private art or Windows visual
acceptance is involved. Windows uses its real ABI with named API redirection.
"""
from pathlib import Path
import argparse, ast, hashlib, json, os, subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default=os.environ.get('CXX','g++'))
p.add_argument('--compiler-arg',action='append',default=[])
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();here=Path(__file__).resolve().parent;root=here.parents[2]
fixture=here/'game_assets_host_test.py';out=a.output.resolve().parent/'building-animation-pixels';out.mkdir(parents=True,exist_ok=True)
values={}
for node in ast.parse(fixture.read_text(encoding='utf-8-sig')).body:
 if isinstance(node,ast.Assign) and len(node.targets)==1 and isinstance(node.targets[0],ast.Name) and node.targets[0].id in ('header','body'):values[node.targets[0].id]=ast.literal_eval(node.value)
prefix,rest=values['body'].split('int main(int argc,char**argv){',1)
if os.name=='nt':
 names=['GetTickCount','GetTickCount64','GetFileAttributesExW','CreateFileW','SetFilePointerEx','ReadFile','CloseHandle','FindFirstFileW','FindNextFileW','FindClose']
 prototypes=values['header'][values['header'].index('DWORD GetTickCount();'):]
 shim='#pragma once\n#include_next <windows.h>\n#include <cstdint>\n'+''.join('#define '+n+' Fixture_'+n+'\n' for n in names)+prototypes
 prefix=prefix.replace('OpenFile','FixtureOpenFile')
else:shim=values['header']
main=r'''
#undef assert
#define assert(...) do { if(!(__VA_ARGS__)) { std::cerr<<"assertion failed at line "<<__LINE__<<": "<<#__VA_ARGS__<<"\n"; return 31; } } while(0)
int main(){
 std::vector<uint8_t> shp(8+24*24+24*18);half(shp,2,4);half(shp,4,3);half(shp,6,24);
 for(int f=0;f<24;++f){int at=8+f*24,data=8+24*24+f*18;half(shp,at+4,4);half(shp,at+6,3);word(shp,at+8,2);word(shp,at+20,data);for(int row=0;row<3;++row){half(shp,data+row*6,6);std::fill(shp.begin()+data+row*6+2,shp.begin()+data+row*6+6,40+f);}}
 std::vector<uint8_t> pal(768,32);for(int i=1;i<256;++i){pal[i*3]=i%64;pal[i*3+1]=(i*2)%64;pal[i*3+2]=(i*3)%64;}
 disk["RA2MD.MIX"]=mix({{"LOCALMD.MIX",mix({{"UNITTEM.PAL",pal}})}});disk["GTCNST_B.SHP"]=shp;
 memcpy(memory.data()+0x7E1B78+78,"TEM",4);memory[0x7E1B78+86]='T';
 constexpr uint32_t building=0xc0000,buildingType=0xc8000,anim=0xd0000,animType=0xd8000,house=0x30000;
 word(memory,building,0x7E3EBC);word(memory,building+0x10,501);word(memory,building+0x6C,100);word(memory,building+0x520,buildingType);word(memory,building+0x21C,house);word(memory,building+0x55C+8*4,anim);
 word(memory,anim,0x7E3354);word(memory,anim+0x10,502);word(memory,anim+0xC8,animType);memory[anim+0x90]=0;memory[anim+0x118]=1;word(memory,anim+0x9C,26112);word(memory,anim+0xA0,21760);word(memory,anim+0xA4,208);word(memory,animType+0x364,2);word(memory,animType+0x2B4,12);word(memory,anim+0xAC,11);
 memcpy(memory.data()+buildingType+0x24,"GACNST",7);memcpy(memory.data()+animType+0x24,"GACNST_B",9);memory[buildingType+0x237]=1;
 gameassets::update();std::shared_ptr<const external::voxel::Image> first;unsigned checkedPixels=0;
 for(int repeat=0;repeat<20;++repeat){
  auto states=external::animations::attached(gameassets::context.read,building,501);assert(states.size()==1&&states[0].frame==23);
  gameassets::ActorArt art;assert(gameassets::captureAnimation(anim,animType,502,building,501,art)&&art.kind==5&&art.filename=="GTCNST_B.SHP");
  external::visual::Pose pose;pose.frame=states[0].frame;external::voxel::Image image;assert(gameassets::renderActor(art,pose,image));assert(image.width==4&&image.height==3&&image.indices==std::vector<uint8_t>(12,63));
  constexpr uint32_t expected=0xfffcf8f4u;assert(art.colors[63]==expected);for(auto pixel:image.pixels){assert(pixel==expected);++checkedPixels;}
  std::shared_ptr<const external::voxel::Image> shared;assert(gameassets::context.renderSharedActor(art,pose,shared)&&shared->pixels==image.pixels);if(first)assert(shared==first);else first=shared;
  assert(word(memory,anim+0xAC)==11); // no host animation advancement
 }
 for(uint32_t flag:{0x81u,0x179u,0x19Du}){memory[anim+flag]=1;gameassets::ActorArt art;assert(!gameassets::captureAnimation(anim,animType,502,building,501,art));memory[anim+flag]=0;}
 gameassets::ActorArt art;word(memory,building+0x55C+8*4,0);assert(!gameassets::captureAnimation(anim,animType,502,building,501,art));
 std::cout<<"PASS held slot: 20 captures, 240 exact RGBA/index pixels, shared immutable material cache, no advance; limbo/dying/invisible/slot-removal rejected\n";return checkedPixels==240?0:31;
}
'''
# The fixture word helper writes vectors, so read this immutable observed value
# through the same production read boundary used by capture.
main=main.replace('assert(word(memory,anim+0xAC)==11);','int observed=-1;assert(gameassets::context.read(anim+0xAC,&observed,4)&&observed==11);')
(out/'windows.h').write_text(shim,encoding='utf-8',newline='\n');(out/'test.cpp').write_text(prefix+main,encoding='utf-8',newline='\n')
files=sorted([*(root/'source/external_reader/source').glob('*.h'),*(root/'source/shared/source').glob('*.h'),root/'source/assist/source/game_assets_host.h',fixture,Path(__file__).resolve(),root/'source/third_party/xbrz/xbrz.cpp'])
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def manifest():return {path.relative_to(root).as_posix():sha(path) for path in files}
before=manifest();compiler=[a.compiler]
if Path(a.compiler).name.lower() in ['zig','zig.exe']:compiler+=['c++']
exe=out/('fixture.exe' if os.name=='nt' else 'fixture')
command=compiler+a.compiler_arg+['-std=c++17','-O2','-UNDEBUG','-I'+str(out),'-I'+str(root),str(out/'test.cpp'),str(root/'source/third_party/xbrz/xbrz.cpp'),'-o',str(exe)]
if os.name=='nt':command+=['-luser32','-lgdi32','-lgdiplus','-lole32','-lshlwapi','-lversion','-lshell32','-lwinmm']
build=subprocess.run(command,text=True,capture_output=True,encoding='utf-8',errors='replace');(out/'build.log').write_text(build.stdout+build.stderr,encoding='utf-8')
run=subprocess.run([str(exe)],text=True,capture_output=True,encoding='utf-8',errors='replace',timeout=60) if not build.returncode else None
after=manifest();report={'passed':build.returncode==0 and run is not None and run.returncode==0 and before==after,'build_exit':build.returncode,'exit':run.returncode if run else None,'source_hash_stable':before==after,'source_sha256':before,'generated_cpp_sha256':sha(out/'test.cpp'),'generated_windows_shim_sha256':sha(out/'windows.h'),'binary_sha256':sha(exe) if exe.exists() else None,'scope':__doc__,'game_tested':False,'windows_visual_tested':False}
if run:report.update(stdout=run.stdout,stderr=run.stderr)
a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:report[k] for k in ['passed','build_exit','exit','source_hash_stable']}))
if build.returncode:print((build.stdout+build.stderr)[-5000:])
elif run:print(run.stdout+run.stderr)
raise SystemExit(not report['passed'])
