// The Python driver supplies only synthetic Win32/files/process boundaries.
// Context, SHP decoding, Layers, SceneRenderer and Surface are production.
#include "radar_scene.h"
#include <iomanip>

using namespace external;
namespace {
constexpr uint32_t Type=0x20000,Owner=0x30000,AnimType=0x40000,Body=0xC00000,Mask=0xD00000,Anim=0xE00000;
unsigned checks=0,failures=0,generation=700;
uint64_t healthyDigest=1469598103934665603ull;
void check(bool ok,const std::string&message){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
template<class T>void digest(const std::vector<T>&values){const auto*p=reinterpret_cast<const uint8_t*>(values.data());for(size_t n=0;n<values.size()*sizeof(T);++n)healthyDigest=(healthyDigest^p[n])*1099511628211ull;}
std::vector<uint8_t> bodyShape(int width=64,int height=96){
 const int frames=2;std::vector<uint8_t>b(8+24*frames);half(b,2,width);half(b,4,height);half(b,6,frames);
 for(int f=0;f<frames;++f){int at=8+24*f;half(b,at+4,width);half(b,at+6,height);word(b,at+8,2);word(b,at+20,b.size());
  for(int row=0;row<height;++row){size_t start=b.size();b.resize(start+width+2);half(b,start,width+2);for(int col=0;col<width;++col)b[start+2+col]=uint8_t(1+(col/8+row/12+f)%3);}}
 return b;
}
void setup(int fw=3,int fh=3,int width=64,int height=96){
 resetFault();disk.clear();std::fill(memory.begin(),memory.end(),0);
 word(memory,Type+0xA4,Body);word(memory,Type+0xEF0,3);memory[Type+0x1702]=1;word(memory,Type+0x1520,13);
 word(memory,0x8192B8+12,fw);word(memory,0x819310+12,fh);word(memory,0x89DDBC,Mask);
 std::memcpy(memory.data()+Type+0x24,"TEST",5);std::memcpy(memory.data()+Type+0x1F8,"TEST",5);
 std::memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);std::memcpy(memory.data()+0x7E1B78+78,"TEM",4);std::memcpy(memory.data()+0x7E1B78+48,"TEMPERAT",9);memory[0x7E1B78+86]='T';
 auto body=bodyShape(width,height);std::memcpy(memory.data()+Body,body.data(),body.size());disk["TEST.SHP"]=body;
 std::vector<uint8_t>mask(32+396*477,1);std::fill_n(mask.begin(),32,0);half(mask,2,396);half(mask,4,477);half(mask,6,1);half(mask,12,396);half(mask,14,477);word(mask,28,32);std::memcpy(memory.data()+Mask,mask.data(),mask.size());
 std::vector<uint8_t>pal(768,32);for(int i=1;i<4;++i){pal[3*i]=uint8_t(40+i*5);pal[3*i+1]=uint8_t(8+i);pal[3*i+2]=5;}disk["ISOTEM.PAL"]=pal;disk["UNITTEM.PAL"]=pal;
 std::vector<uint8_t>animPal(768,0);for(int i=1;i<256;++i)animPal[3*i+1]=45;disk["ANIM.PAL"]=animPal;
 word(memory,AnimType+0xA4,Anim);memory[AnimType+0x35C]=1;memory[AnimType+0x369]=1;word(memory,0xA8EB78,2);
 std::memcpy(memory.data()+AnimType+0x24,"ANIMTEST",9);std::memcpy(memory.data()+AnimType+0x1F8,"ANIMTEST",9);
 auto anim=bodyShape(32,32);std::memcpy(memory.data()+Anim,anim.data(),anim.size());disk["ANIMTEST.SHP"]=anim;
 word(memory,0xA8B230,0x50000);word(memory,0x50000+0x352C,100);for(int i=0;i<3;++i)word(memory,0x50000+0x3534+i*4,100);
}
void initialize(gameassets::Context&assets,uint32_t match){assets.identity(pid,match);assets.presentationLighting(false,false);}
radar::Frame makeFrame(bool animation=false,bool flat=false){
 radar::Frame f;auto t=std::make_shared<radar::Terrain>();t->geo={8,8,4,4,8,16,1,0x60000,Owner,512*512,{8,8,0,0,8,8},8,16};
 t->art.resize(512*512);t->colors.resize(512*512);t->baseColors.resize(512*512);f.terrain=t;f.frame=1;f.tick=100;f.markerScanValid=true;f.posesPrepared=true;
 auto e=std::make_shared<sceneextras::Snapshot>();e->frozenValid=true;sceneextras::Frozen b;b.object=0x10000;b.id=42;b.owner=Owner;b.point={6*256,6*256,0};b.records.push_back({Type,0,int(flat),0});if(animation)b.records.push_back({AnimType,0,0,0});e->frozen.push_back(b);f.extras=e;return f;
}
struct Draw {bool ready=false;camera::Surface surface;std::vector<std::shared_ptr<const voxel::Image>>images;};
Draw draw(sceneextras::Layers&layers,gameassets::Context&assets,const radar::Frame&frame,bool begin=true){
 Draw r;r.surface.reset(240,240,0xff102030);if(begin)layers.begin();
 r.ready=layers.frozen(frame,assets.read,assets,lighting::State{},1,[&](const voxel::Image&i,float x,float y,float sx,float sy,float base,const lighting::State&l,float lift,float drop,std::shared_ptr<const voxel::Image>owner,vulkancompute::MotionIdentity,camera::Material m){r.images.push_back(owner);r.surface.blit(i,x,y,sx,sy,base,l,lift,drop,false,m);});return r;
}
bool sameImage(const voxel::Image&a,const voxel::Image&b){return a.width==b.width&&a.height==b.height&&a.x==b.x&&a.y==b.y&&a.pixels==b.pixels&&a.indices==b.indices&&a.depth==b.depth&&a.nativeDepth==b.nativeDepth&&a.nativeShape==b.nativeShape&&a.nativeShapeCoverage==b.nativeShapeCoverage&&a.depthBias==b.depthBias&&a.flat==b.flat&&a.shadow==b.shadow&&a.writeDepth==b.writeDepth;}
bool sameDraw(const Draw&a,const Draw&b){if(a.images.size()!=b.images.size()||a.surface.color!=b.surface.color||a.surface.nativeZ!=b.surface.nativeZ)return false;for(size_t n=0;n<a.images.size();++n)if(!sameImage(*a.images[n],*b.images[n]))return false;return true;}
size_t differences(const std::vector<uint32_t>&a,const std::vector<uint32_t>&b){if(a.size()!=b.size())return ~size_t(0);size_t n=0;for(size_t i=0;i<a.size();++i)n+=a[i]!=b[i];return n;}
struct Missing {const char*name;uint32_t address;size_t bytes;};
const Missing missing[]={
 {"foundation-index",Type+0xEF0,4},{"foundation-width",0x8192B8+12,4},{"foundation-height",0x819310+12,4},
 {"move-x",Type+0x1530,4},{"move-y",Type+0x1534,4},{"normal-z",Type+0x1520,4},
 {"mask-pointer",0x89DDBC,4},{"mask-header",Mask,8},{"mask-row",Mask+32,4096}
};
void recoveryChecks(){
 size_t mismatches=0;
 for(const auto&fault:missing){
  setup();word(memory,Type+0x1530,5);word(memory,Type+0x1534,7);auto frame=makeFrame();gameassets::Context assets(externalReader),fresh(externalReader);uint32_t match=++generation;initialize(assets,match);initialize(fresh,match);sceneextras::Layers recovered,reference;
  auto healthy=draw(reference,fresh,frame);check(healthy.ready&&healthy.images.size()==2,std::string(fault.name)+" healthy body and shadow");
  setFault(fault.address,fault.bytes);auto failed=draw(recovered,assets,frame);size_t hits=faultHits;check(hits>0,std::string(fault.name)+" exercises read failure");check(!failed.ready,std::string(fault.name)+" incomplete layer rejected");
  resetFault();auto after=draw(recovered,assets,frame);size_t diff=differences(after.surface.color,healthy.surface.color);mismatches+=diff;
  check(after.ready&&sameDraw(after,healthy),std::string(fault.name)+" same cache recovers to healthy pixels and native depth");
  auto warm=draw(recovered,assets,frame);check(warm.ready&&warm.images==after.images&&sameDraw(warm,after),std::string(fault.name)+" recovered image ownership remains reusable");
  digest(healthy.surface.color);digest(healthy.surface.nativeZ);
  std::cout<<fault.name<<" failure_ready="<<failed.ready<<" fault_hits="<<hits<<" recovery_ready="<<after.ready<<" pixel_mismatches="<<diff<<'\n';
 }
 std::cout<<"layer_recoveries="<<std::size(missing)<<" pixel_mismatches="<<mismatches<<'\n';
}
void optionalChecks(){
 for(bool flat:{false,true}){
  setup(flat?3:8);auto frame=makeFrame(false,flat);gameassets::Context assets(externalReader);initialize(assets,++generation);sceneextras::Layers layers;
  setFault(0x89DDBC,4);auto image=draw(layers,assets,frame);check(image.ready&&image.images.size()==2,flat?"flat saved body remains valid":"wide saved body remains valid");check(faultHits==0,"unused cone pointer is not read");digest(image.surface.color);resetFault();
 }
 setup();word(memory,0x89DDBC,0);gameassets::Context absent(externalReader);initialize(absent,++generation);sceneextras::Layers layers;auto empty=draw(layers,absent,makeFrame());
 check(empty.ready&&empty.images.size()==2&&empty.images[0]->nativeShapeCoverage.empty(),"verified null mask remains valid");digest(empty.surface.color);
 setup();gameassets::Context malformed(externalReader),fresh(externalReader);uint32_t match=++generation;initialize(malformed,match);initialize(fresh,match);sceneextras::Layers recovered,reference;
 half(memory,Mask+2,0);auto failed=draw(recovered,malformed,makeFrame());check(!failed.ready,"nonzero malformed mask cannot become absent cached mask");half(memory,Mask+2,396);
 auto after=draw(recovered,malformed,makeFrame()),healthy=draw(reference,fresh,makeFrame());check(after.ready&&healthy.ready&&sameDraw(after,healthy),"malformed mask recovers without resource revision");digest(healthy.surface.color);
 // A failed animation palette selector must not seed this presentation's
 // palette cache. Retry before begin() clears that cache as an extra guard.
 for(const auto&fault:std::vector<Missing>{{"animation-palette",AnimType+0x35C,1},{"animation-flat",AnimType+0x369,1},{"body-palette",Type+0x1702,1}}){
  setup();gameassets::Context assets(externalReader),clean(externalReader);uint32_t id=++generation;initialize(assets,id);initialize(clean,id);sceneextras::Layers retry,control;auto frame=makeFrame(true);
  auto good=draw(control,clean,frame);check(good.ready&&good.images.size()==3,std::string(fault.name)+" healthy saved animation");
  setFault(fault.address,fault.bytes);auto bad=draw(retry,assets,frame);check(faultHits>0&&!bad.ready,std::string(fault.name)+" failed metadata is rejected");resetFault();
  auto next=draw(retry,assets,frame,false);check(next.ready&&sameDraw(next,good),std::string(fault.name)+" retry in same presentation restores correct art");digest(good.surface.color);digest(good.surface.nativeZ);
 }
}
void sceneChecks(){
 size_t mismatches=0;unsigned scenes=0;
 for(int variant=0;variant<2;++variant)for(unsigned n:{0u,1u,3u,5u,6u,8u}){
  setup(3,variant?2:3,variant?80:64,variant?72:96);word(memory,Type+0x1530,5);auto frame=makeFrame();gameassets::Context assets(externalReader),clean(externalReader);uint32_t match=++generation;initialize(assets,match);initialize(clean,match);
  radar::SceneRenderer<gameassets::Context>renderer,control;renderer.outputSized(true);control.outputSized(true);const auto&fault=missing[n];
  radar::SceneRegion region{variant?20.25:0,variant?25.5:0,variant?180.:240.,variant?180.:240.,true};
  auto paint=[&](auto&r,auto&a){return r.render(frame,240,240,a.read,a,region,1,true,false,false,true);};
  setFault(fault.address,fault.bytes);auto bad=paint(renderer,assets);check(faultHits>0,std::string(fault.name)+" actual scene exercises read failure");check(!bad,std::string(fault.name)+" actual SceneRenderer rejects incomplete saved art");resetFault();
  ++frame.frame;frame.tick+=20;auto after=paint(renderer,assets),healthy=paint(control,clean);check(after&&healthy,std::string(fault.name)+" actual SceneRenderer recovers");
  size_t diff=after&&healthy?differences(after->pixels,healthy->pixels):~size_t(0);if(diff!=~size_t(0))mismatches+=diff;check(diff==0,std::string(fault.name)+" retained scene equals complete reference");++scenes;if(healthy)digest(healthy->pixels);
  std::cout<<"scene_"<<fault.name<<" variant="<<variant<<" failure_scene="<<bool(bad)<<" recovery_scene="<<bool(after)<<" pixel_mismatches="<<diff<<'\n';
 }
 std::cout<<"scene_recoveries="<<scenes<<" pixel_mismatches="<<mismatches<<'\n';
}
}
int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);xbrzadapter::gpuPresentation(false);
 recoveryChecks();optionalChecks();sceneChecks();
 std::cout<<"healthy_digest="<<std::hex<<healthyDigest<<std::dec<<" checks="<<checks<<" failures="<<failures<<'\n';
 if(!failures)std::cout<<"PASS transactional frozen body/animation palette and depth caches, same-cache recovery, legal absent/wide/flat paths and actual SceneRenderer output\n";
 return failures?1:0;
}
