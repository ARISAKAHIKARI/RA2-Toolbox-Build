// The Python driver supplies only synthetic file/process boundaries. Every
// capture, decode, cache, SceneRenderer and native depth operation is production.
#include "source/external_reader/source/radar_scene.h"
#include <iomanip>

using namespace external;
static unsigned checks=0,failures=0;
static uint64_t healthyDigest=1469598103934665603ull;
template<class T>static void digest(const std::vector<T>&values){const auto*p=reinterpret_cast<const uint8_t*>(values.data());for(size_t n=0;n<values.size()*sizeof(T);++n){healthyDigest^=p[n];healthyDigest*=1099511628211ull;}}
static void check(bool ok,const std::string&message){++checks;if(!ok){++failures;std::cerr<<"FAIL "<<message<<'\n';}}
static constexpr uint32_t Actor=0x10000,Type=0x20000,Owner=0x30000,Tile=0x40000,Body=0xC00000,Mask=0xD00000;
static uint32_t generation=400;

static std::vector<uint8_t> bodyShape(int width=64,int height=96,bool compressed=true){
 const int frames=4;std::vector<uint8_t>b(8+24*frames);half(b,2,width);half(b,4,height);half(b,6,frames);
 for(int f=0;f<frames;++f){int w=width-(f&1)*4,h=height-(f&1)*6,x=(f&1)*2,y=(f&1)*3,at=8+24*f;
  half(b,at,x);half(b,at+2,y);half(b,at+4,w);half(b,at+6,h);word(b,at+8,compressed?2:0);word(b,at+20,b.size());
  for(int row=0;row<h;++row){size_t start=b.size();b.resize(start+w+(compressed?2:0));if(compressed)half(b,start,w+2);
   for(int col=0;col<w;++col)b[start+(compressed?2:0)+col]=uint8_t(1+(col/8+row/12+f)%3);}
 }return b;
}
static std::vector<uint8_t> depthShape(bool biased=true){
 std::vector<uint8_t>b(32+396*477,biased?1:66);std::fill_n(b.begin(),32,0);half(b,2,396);half(b,4,477);half(b,6,1);half(b,12,396);half(b,14,477);word(b,28,32);return b;
}
static std::vector<uint8_t> groundShape(){
 std::vector<uint8_t>b(20+52+1800,0);word(b,0,1);word(b,4,1);word(b,8,60);word(b,12,30);word(b,16,20);word(b,32,952);word(b,56,2);
 std::fill(b.begin()+72,b.begin()+972,5);return b;
}
static void setup(int fw=3,int fh=3,int width=64,int height=96,bool compressed=true,bool custom=false,bool flat=false){
 resetFault();disk.clear();std::fill(memory.begin(),memory.end(),0);
 word(memory,Actor,0x7E3EBC);word(memory,Actor+0x10,42);word(memory,Actor+0x6C,100);word(memory,Actor+0x520,Type);word(memory,Actor+0x21C,Owner);word(memory,Actor+0x534,1);memory[Actor+0x6E4]=1;
 std::memcpy(memory.data()+Type+0x24,"TEST",5);std::memcpy(memory.data()+Type+0x1F8,"TEST",5);word(memory,Type+0xA4,Body);word(memory,Type+0xEF0,custom?uint32_t(-1):3);
 word(memory,0x8192B8+12,fw);word(memory,0x819310+12,fh);word(memory,0x89DDBC,Mask);
 std::memcpy(memory.data()+0x7E1B78+68,"ISOTEM",7);std::memcpy(memory.data()+0x7E1B78+78,"TEM",4);std::memcpy(memory.data()+0x7E1B78+48,"TEMPERAT",9);memory[0x7E1B78+86]='T';
 std::memcpy(memory.data()+Tile+0x2F5,"CLEAR01.TEM",12);
 auto body=bodyShape(width,height,compressed),mask=depthShape();std::memcpy(memory.data()+Body,body.data(),body.size());std::memcpy(memory.data()+Mask,mask.data(),mask.size());disk["TEST.SHP"]=std::move(body);disk["CLEAR01.TEM"]=groundShape();
 auto&unit=disk["UNITTEM.PAL"];unit.assign(768,32);for(int i=1;i<4;++i){unit[3*i]=uint8_t(40+i*5);unit[3*i+1]=uint8_t(8+i);unit[3*i+2]=5;}auto&iso=disk["ISOTEM.PAL"];iso.assign(768,0);for(int i=1;i<256;++i){iso[3*i]=8;iso[3*i+1]=24;iso[3*i+2]=12;}
 const std::string rules="[TEST]\nNoShadow=yes\n";
 const std::string art="[TEST]\nFoundation="+(custom?std::string("Custom\nFoundation.X=")+std::to_string(fw)+"\nFoundation.Y="+std::to_string(fh):std::to_string(fw)+"x"+std::to_string(fh))+"\nNormalZAdjust=13\n"+(flat?"Flat=yes\n":"");
 disk["RULESMD.INI"]={rules.begin(),rules.end()};disk["ARTMD.INI"]={art.begin(),art.end()};
}
static void initialize(gameassets::Context&context,uint32_t match=0){context.identity(pid,match?match:++generation);context.presentationLighting(false,false);}
static bool sameImage(const voxel::Image&a,const voxel::Image&b){return a.width==b.width&&a.height==b.height&&a.x==b.x&&a.y==b.y&&a.pixels==b.pixels&&a.depth==b.depth&&a.nativeDepth==b.nativeDepth&&a.nativeShape==b.nativeShape&&a.nativeShapeCoverage==b.nativeShapeCoverage;}
static bool capture(gameassets::Context&context,gameassets::ActorArt&art){return context.captureActor(Actor,Type,42,art);}
struct Missing {const char*name;uint32_t address;size_t bytes;bool live=false;unsigned skip=0;};
static const Missing missing[]={
 {"foundation-page",0x819000,4096},{"foundation-index",Type+0xEF0,4},
 {"foundation-width",0x8192B8+12,4},{"foundation-height",0x819310+12,4},
 {"move-x",Type+0x1530,4},{"move-y",Type+0x1534,4},{"z-adjust",Type+0x1520,4},
 {"mask-pointer",0x89DDBC,4},{"mask-resource",Mask+32,4096,true},
 {"late-actor-identity",Actor+0x10,4,false,1}
};
static void transactionChecks(){
 for(const auto&fault:missing){setup();gameassets::Context context(externalReader);initialize(context);gameassets::ActorArt art;art.type=0x123456;art.fw=27;
  auto oldRevision=context.maskRevision;auto oldMask=context.buildingMask;setFault(fault.address,fault.bytes,fault.live,fault.skip);
  bool ready=capture(context,art);check(!ready,std::string(fault.name)+" rejects unavailable metadata");check(art.type==0x123456&&art.fw==27,std::string(fault.name)+" leaves output untouched");
  check(context.maskRevision==oldRevision&&context.buildingMask==oldMask,std::string(fault.name)+" leaves mask transaction untouched");check(faultHits>0,std::string(fault.name)+" exercises real read failure");
  resetFault();check(capture(context,art),std::string(fault.name)+" recovers on the same Context");check(art.fw==3&&art.fh==3&&art.depthMask&&art.depthMaskBiased,std::string(fault.name)+" restores complete native depth binding");
  voxel::Image image;visual::Pose pose;check(context.renderActor(art,pose,image),std::string(fault.name)+" renders recovered art");check(image.nativeShapeCoverage.size()==6144&&std::count(image.nativeShapeCoverage.begin(),image.nativeShapeCoverage.end(),uint8_t(0))==0,std::string(fault.name)+" preserves all 6144 body pixels");
 }
 setup();gameassets::Context malformed(externalReader);initialize(malformed);gameassets::ActorArt art;word(memory,0x8192B8+12,0);check(!capture(malformed,art),"invalid native foundation cannot become cached 1x1");word(memory,0x8192B8+12,3);check(capture(malformed,art),"invalid native foundation recovers after repair");
 for(unsigned index:{4u,9u}){setup();gameassets::Context retained(externalReader);initialize(retained);gameassets::ActorArt ready;check(capture(retained,ready),"warm valid mask cache");auto owner=retained.buildingMask;auto revision=retained.maskRevision;const auto&fault=missing[index];setFault(fault.address,fault.bytes,fault.live,fault.skip);
  check(!capture(retained,ready)&&retained.buildingMask==owner&&retained.maskRevision==revision&&ready.depthMask==owner,"later actor failure preserves existing valid mask and output");resetFault();check(capture(retained,ready)&&ready.depthMask==owner,"same cached mask remains usable after actor recovery");}
 std::cout<<"transactions="<<std::size(missing)<<"\n";
}
static void fallbackChecks(){
 for(int index:{-1,21,127}){setup(3,2,64,72,true,true);word(memory,Type+0xEF0,uint32_t(index));gameassets::Context c(externalReader);initialize(c);gameassets::ActorArt a;
  setFault(0x819000,4096);check(capture(c,a)&&a.fw==3&&a.fh==2,"Custom/extended enum keeps INI foundation");check(faultHits==0,"Custom/extended enum does not read native dimension table");resetFault();}
 for(bool flat:{false,true}){setup(flat?3:8,3,64,96,true,false,flat);gameassets::Context c(externalReader);initialize(c);gameassets::ActorArt a;setFault(0x89DDBC,4);
  check(capture(c,a)&&!a.depthMask,"wide/Flat building does not require unused cone");check(faultHits==0,"wide/Flat building skips BUILDNGZ pointer");resetFault();}
 setup();word(memory,0x89DDBC,0);gameassets::Context absent(externalReader);initialize(absent);gameassets::ActorArt empty;check(capture(absent,empty)&&!empty.depthMask,"verified null and no disk mask remains valid");
 setup();word(memory,0x89DDBC,0);disk["BUILDNGZ.SHA"]=depthShape(false);gameassets::Context fromDisk(externalReader);initialize(fromDisk);gameassets::ActorArt diskArt;check(capture(fromDisk,diskArt)&&diskArt.depthMask&&!diskArt.depthMaskBiased,"disk BUILDNGZ bias path remains valid");
 voxel::Image diskImage;visual::Pose pose;check(fromDisk.renderActor(diskArt,pose,diskImage),"disk BUILDNGZ renders");
 setup();gameassets::Context native(externalReader);initialize(native);gameassets::ActorArt nativeArt;voxel::Image nativeImage;check(capture(native,nativeArt)&&native.renderActor(nativeArt,pose,nativeImage)&&sameImage(diskImage,nativeImage),"disk/native BUILDNGZ produce identical depth and coverage");
 digest(nativeImage.pixels);digest(nativeImage.depth);digest(nativeImage.nativeShape);digest(nativeImage.nativeShapeCoverage);
 setup(3,3,64,96,false);gameassets::Context raw(externalReader);initialize(raw);gameassets::ActorArt rawArt;voxel::Image rawImage;check(capture(raw,rawArt)&&raw.renderActor(rawArt,pose,rawImage)&&!rawImage.nativeDepth.shape,"raw SHP preserves clip-only BUILDNGZ rule");
 digest(rawImage.pixels);digest(rawImage.depth);digest(rawImage.nativeShape);digest(rawImage.nativeShapeCoverage);
 setup();word(memory,Type+0x1530,5000);word(memory,Type+0x1534,uint32_t(-5000));word(memory,Type+0x1520,9000);gameassets::Context clamp(externalReader);initialize(clamp);gameassets::ActorArt limited;check(capture(clamp,limited)&&limited.moveX==4096&&limited.moveY==-4096&&limited.zAdjust==13,"successful outlier reads retain existing clamp/INI policy");
}
static std::shared_ptr<radar::Terrain> makeTerrain(){
 auto t=std::make_shared<radar::Terrain>();t->geo={8,8,4,4,8,16,1,0x60000,Owner,512*512,{8,8,0,0,8,8},8,16};t->reveal=true;t->colors.resize(512*512);t->art.resize(512*512);t->shade.resize(512*512);
 for(int y=4;y<8;++y)for(int x=4;x<8;++x){size_t at=y*512+x;t->colors[at]=0x204020;t->art[at].type=Tile;}return t;
}
static radar::Frame makeFrame(){radar::Frame f;f.terrain=makeTerrain();f.tick=100;f.frame=1;f.posesPrepared=true;
 radar::Marker m;m.object=Actor;m.type=Type;m.owner=Owner;m.id=42;m.x=m.y=6*256;m.kind=3;m.building=true;m.layer=2;m.pose.variant=-1;m.pose.buildingFlags=buildings::Constructed;f.markers={m};return f;}
static void sceneChecks(){
 size_t totalDifferences=0;unsigned scenes=0;
 for(int variant=0;variant<3;++variant)for(unsigned faultIndex:{0u,4u,6u,7u,8u,9u}){
  setup(variant==2?4:3,variant==1?2:3,variant==2?80:64,variant==1?72:96);auto frame=makeFrame();gameassets::Context warmed(externalReader),fresh(externalReader);uint32_t match=++generation;initialize(warmed,match);initialize(fresh,match);
  radar::SceneRenderer<gameassets::Context> wr,fr;const auto&fault=missing[faultIndex];setFault(fault.address,fault.bytes,fault.live,fault.skip);
  const radar::SceneRegion full{0,0,240,240,true},crop{30.25,40.5,160,160,true};auto first=wr.render(frame,240,240,warmed.read,warmed,full,1,true,false,false,true);
  check(!wr.materialsReady(),std::string(fault.name)+" incomplete first scene cannot publish ready art");resetFault();
  frame.tick+=20;++frame.frame;auto region=variant?crop:full;int pixels=variant?320:240;
  for(int pose=0;pose<2;++pose){frame.markers.front().pose.frame=pose;frame.tick+=20;++frame.frame;
   auto recovered=wr.render(frame,pixels,pixels,warmed.read,warmed,region,1,true,false,false,true);auto reference=fr.render(frame,pixels,pixels,fresh.read,fresh,region,1,true,false,false,true);
   check(recovered&&reference&&wr.materialsReady()&&fr.materialsReady(),std::string(fault.name)+" scene recovers without revision/camera reset");
   size_t differences=0;if(recovered&&reference&&recovered->pixels.size()==reference->pixels.size())for(size_t n=0;n<reference->pixels.size();++n)differences+=recovered->pixels[n]!=reference->pixels[n];else differences=~size_t(0);
   check(differences==0,std::string(fault.name)+" recovered cached scene equals fresh complete scene");if(differences!=~size_t(0))totalDifferences+=differences;++scenes;
   if(reference)digest(reference->pixels);
  }
 }
 std::cout<<"recovered_scenes="<<scenes<<" pixel_mismatches="<<totalDifferences<<"\n";
}
static void nativeCompositionChecks(){
 setup();gameassets::Context c(externalReader);initialize(c);gameassets::ActorArt art;check(capture(c,art),"composition captures production building");visual::Pose pose;voxel::Image image;check(c.renderActor(art,pose,image),"composition renders production building");
 // Three independent strips exercise far ground, a foreground cliff, and a
 // slope crossing the standing sprite. Tile candidates differ by their real
 // decoded TMP Z bytes, not by disabled depth tests or draw-on-top flags.
 std::array<uint32_t,256>pal{};pal.fill(0xff207040);pal[0]=0;voxel::Image ground;check(assets::tmp(groundShape(),0,pal,ground),"composition decodes production TMP");
 constexpr int W=200,H=200;auto base=std::make_shared<const std::vector<uint32_t>>(W*H,0xff102030);auto z=std::make_shared<const std::vector<float>>(W*H,-10000.f);
 camera::GpuSceneRecord record;record.begin(W,H,base,z,colorprecision::Mode::NativeSteps);record.nativeView(1,1,true);lighting::State light;light.intensity=1000;camera::Material material;material.nativeZAdjust=-2;
 auto tile=std::make_shared<const voxel::Image>(ground);auto building=std::make_shared<const voxel::Image>(image);
 check(record.append(*tile,tile,80,120,1,1,0,light,1,0,false,{},material),"append far ground");
 check(record.append(*tile,tile,120,145,1,1,0,light,1,0,false,{},material),"append near cliff");
 voxel::Image slope=ground;for(size_t n=0;n<slope.nativeShape.size();++n){slope.nativeShape[n]=(int(n%slope.width)<30?0:48);if(std::isfinite(slope.depth[n]))slope.depth[n]=-float(slope.nativeShape[n]);}
 auto slopeOwner=std::make_shared<const voxel::Image>(std::move(slope));check(record.append(*slopeOwner,slopeOwner,110,115,1,1,0,light,1,0,false,{},material),"append slope");
 check(record.append(*building,building,100,90,1,1,0,light,1,0,false,{},material),"append standing building");
 camera::Surface onlyBuilding;check(onlyBuilding.reset(W,H,0xff102030),"reset building reference");onlyBuilding.nativeBase(std::make_shared<const std::vector<uint8_t>>(W*H,127));onlyBuilding.blit(image,100,90,1,1,0,light,1,0,false,material);
 camera::Surface replay;check(record.replay(replay),"CPU replay of shared GPU record");size_t shown=0,hidden=0;
 for(size_t n=0;n<replay.color.size();++n)if(onlyBuilding.color[n]!=0xff102030){shown+=replay.color[n]==onlyBuilding.color[n];hidden+=replay.color[n]!=onlyBuilding.color[n];}
 check(shown>1000&&hidden>0,"normal terrain and building occlusion both remain active");
 const auto&draw=record.frame().draws.back();check(draw.nativeDepth==image.nativeDepth&&draw.nativeShape&&*draw.nativeShape==image.nativeShape&&draw.nativeCoverage&&*draw.nativeCoverage==image.nativeShapeCoverage,"GPU packet uses same checked depth shape and coverage");
 auto expected=nativesurface::walk(image.nativeDepth,42,96,H,-2);check(draw.nativeWalk==expected,"CPU/GPU native walk uses the same origin, crop and adjust");
 // Equal-depth later bodies stay hidden; a genuinely nearer body can cover
 // the first. The production standing-image comparison remains strict.
 auto other=art;other.colors.fill(0xff2050e0);voxel::Image otherImage;check(c.renderActor(other,pose,otherImage),"render overlap building");auto farther=std::make_shared<const voxel::Image>(otherImage);
 auto before=replay.color;check(record.append(*farther,farther,100,90,1,1,0,light,1,0,false,{},material)&&record.replay(replay)&&replay.color==before,"equal depth building cannot paint through existing body");
 check(record.append(*farther,farther,100,110,1,1,0,light,1,0,false,{},material)&&record.replay(replay)&&replay.color!=before,"nearer building still occludes farther building");
 digest(replay.color);digest(replay.nativeZ);
 vulkancompute::FrameResolve gpu;std::vector<uint32_t>gpuPixels;const bool rendered=gpu.renderFrame(record.frame(),gpuPixels);if(rendered)check(gpuPixels==replay.color,"actual Vulkan output equals CPU native replay");
 std::cout<<"native_visible="<<shown<<" native_occluded="<<hidden<<" vulkan="<<(rendered?"executed":"unavailable")<<" reason="<<gpu.diagnostics().reason<<"\n";
}
int main(){transactionChecks();fallbackChecks();sceneChecks();nativeCompositionChecks();std::cout<<"healthy_digest="<<std::hex<<healthyDigest<<std::dec<<" checks="<<checks<<" failures="<<failures<<"\n";return failures?1:0;}
