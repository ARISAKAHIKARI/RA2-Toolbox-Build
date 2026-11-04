#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef CAMERA_ASSETS_HEADER
#define CAMERA_ASSETS_HEADER "../../external_reader/source/camera_assets.h"
#endif
#include CAMERA_ASSETS_HEADER
#include "gpu_voxel_cases.h"
#include <chrono>
#include <cstdio>
using namespace external;
struct Reader {
 bool fail=false,race=false;unsigned tables=0;uint32_t count=32;std::vector<uint8_t>table=std::vector<uint8_t>(8192);
 Reader(){for(size_t i=0;i<table.size();++i)table[i]=uint8_t(1+(i+i/256*7)%255);}
 bool imagePath(wchar_t*,DWORD*){return false;}
 bool readFresh(uint32_t at,void*out,size_t n){if(fail)return false;
  if(at==0xB2FB58&&n==4){memcpy(out,&count,4);return true;}
  if(at==0xB41178&&n==table.size()){memcpy(out,table.data(),n);if(race&&!(++tables&1))static_cast<uint8_t*>(out)[17]^=1;return true;}return false;
 }
};
using Context=cameraassets::Context<Reader>;
static uint64_t digest=1469598103934665603ull;static volatile uint64_t consumed=0;
static void hash(const void*p,size_t n){auto*b=static_cast<const uint8_t*>(p);while(n--)digest=(digest^*b++)*1099511628211ull;}
static void imageHash(const voxel::Image&i){
 for(int v:{i.width,i.height,i.x,i.y,i.depthBottom,i.rasterScale,int(i.flat),int(i.writeDepth),int(i.shadow),int(i.nativeDepth.enabled),int(i.nativeDepth.voxel),int(i.nativeDepth.gradient),i.nativeDepth.adjust})hash(&v,4);
 hash(i.pixels.data(),i.pixels.size()*4);hash(i.depth.data(),i.depth.size()*4);hash(i.indices.data(),i.indices.size());
 if(i.gpuRaster){const auto&p=*i.gpuRaster;hash(p.triangles.data(),p.triangles.size()*sizeof(gpuvoxel::Triangle));}
 if(i.sourcePalette)hash(i.sourcePalette->data(),i.sourcePalette->size()*4);
 for(const auto&a:i.attachments)imageHash(a);
}
static double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
static voxel::Model model(){
 voxel::Model out;const int step[6][3]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}},corners[6][4]={{0,4,6,2},{1,3,7,5},{0,1,5,4},{2,6,7,3},{0,2,3,1},{4,5,7,6}};
 auto block=[&](int sx,int sy,int sz,float ox,float oy,float oz,int part){auto motion=std::make_shared<voxel::Motion>();
  for(int frame=0;frame<8;++frame){auto m=voxelposture::identity();float a=float(frame)*.017f;const float c=std::cos(a),s=std::sin(a);m[0]=c;m[2]=s;m[8]=-s;m[10]=c;m[3]=part?frame*.08f:0;m[11]=frame*.025f;motion->changes.push_back(m);}
  for(int z=0;z<sz;++z)for(int y=0;y<sy;++y)for(int x=0;x<sx;++x)for(int side=0;side<6;++side){int nx=x+step[side][0],ny=y+step[side][1],nz=z+step[side][2];if(nx>=0&&nx<sx&&ny>=0&&ny<sy&&nz>=0&&nz<sz)continue;
   voxel::Face f;f.color=uint8_t(1+(x*13+y*7+z*3+part*31)%254);f.normalType=4;f.normalIndex=uint8_t((side*37+part*11)%253);f.hasNormal=true;auto n=voxel::Normals4[f.normalIndex];f.normal={n[0],-n[1],n[2]};f.part=uint8_t(part);f.turret=part>0;f.motion=motion;
   for(int c=0;c<4;++c){int v=corners[side][c];f.p[c]={ox+x+float(v&1),oy+y+float((v>>1)&1),oz+z+float((v>>2)&1)};}out.faces.push_back(f);
  }
 };
 block(28,16,8,-14,-8,0,0);block(12,10,7,-6,-5,8,1);block(18,3,3,3,-1.5f,12,2);return out;
}
static Context::ActorArt art(){Context::ActorArt a;a.voxel=true;a.kind=1;a.geometryIdentity=std::make_shared<const uint8_t>(0);a.model=model();a.turretOffset=3.5f;
 for(unsigned i=1;i<256;++i)a.colors[i]=0xff000000u|(i*37%256)<<16|(i*79%256)<<8|(i*113%256);return a;}
static visual::Pose pose(int n){visual::Pose p;p.bodyFine=(n*3+13)%256;p.turretFine=(n*7+29)%256;p.hvaBody=n%2?0:1+n%3;p.hvaTurret=1+n%3;p.rawBarrel=uint16_t((8+n%4)<<11);p.recoil={float(n%5)*.09f,float(n%7)*.05f};p.posture.active=n%3!=0;p.posture.sideways=float(n%7-3)*.009f;p.posture.forwards=float(n%5-2)*.013f;return p;}
static double median(std::vector<double>v){std::sort(v.begin(),v.end());return v[v.size()/2];}
int main(){Reader reader;Context c(reader);c.identity(100,1);c.presentationDetail(1);assert(c.presentationResolution(640,480,960,720));auto a=art();unsigned checked=0;
 // CPU and GPU packet paths see changing angles, HVA, posture, palette, VPL,
 // normal fallback and shadow state; completed-image caches are bypassed.
 for(bool gpu:{false,true}){c.presentationGPU(gpu);for(int n=0;n<32;++n){
  auto p=pose(n);c.beginPresentation();voxel::Image im;assert(c.renderActor(a,p,im));assert(im.attachments.size()==1);imageHash(im);++checked;
  a.colors[1+(n*7)%255]^=0x10203;reader.table[(n*263)%8192]^=1;
  if(n==10){auto replaced=a;replaced.geometryIdentity=std::make_shared<const uint8_t>(0);replaced.model.faces[0].p[0].x+=.25f;voxel::Image next;assert(c.renderActor(replaced,p,next));imageHash(next);++checked;}
 }
 // Failed fresh light may not use cached transformed geometry as stale light.
 reader.fail=true;c.beginPresentation();voxel::Image failed;assert(!c.renderActor(a,pose(1),failed));reader.fail=false;reader.race=true;reader.tables=0;c.beginPresentation();assert(!c.renderActor(a,pose(1),failed));reader.race=false;c.beginPresentation();assert(c.renderActor(a,pose(1),failed));imageHash(failed);++checked;
 // VoxelAnim ramp is applied before flattening, unlike an ordinary unit's
 // shadow projection. Its world posture must not enter the reusable HVA model.
 a.kind=voxelanims::Kind;auto p=pose(7);p.posture.ramp={{1,0,0,0,0,.98f,-.2f,0,0,.2f,.98f,0}};assert(c.renderActor(a,p,failed));imageHash(failed);++checked;a.kind=1;
 auto fallback=a;fallback.geometryIdentity=std::make_shared<const uint8_t>(0);fallback.model.faces[0].hasNormal=false;fallback.model.faces[0].normalIndex=0;assert(c.renderActor(fallback,p,failed));imageHash(failed);++checked;
 p=pose(0);p.hvaBody=p.hvaTurret=0;a.shadow=false;assert(c.renderActor(a,p,failed)&&failed.attachments.empty());imageHash(failed);++checked;a.shadow=true;
 auto rigid=a;rigid.geometryIdentity=std::make_shared<const uint8_t>(0);for(auto&f:rigid.model.faces)f.motion.reset();
 p=pose(0);p.hvaBody=p.hvaTurret=0;assert(c.renderActor(rigid,p,failed));imageHash(failed);++checked;
 p=pose(4);assert(c.renderActor(rigid,p,failed));imageHash(failed);++checked;
 p.posture.available=false;assert(!c.renderActor(a,p,failed));p=pose(2);p.gunPoseValid=false;assert(!c.renderActor(a,p,failed));
 // A building SHP body plus separately captured turret/barrel geometry uses
 // each gun's own identity and HVA frame. Include per-draw recoil and palette.
 Context::ActorArt building;building.kind=3;building.colors=a.colors;building.shadow=false;building.shape.resize(36);
 auto put=[&](size_t at,uint16_t v){memcpy(building.shape.data()+at,&v,2);};put(2,2);put(4,2);put(6,1);put(12,2);put(14,2);uint32_t off=32;memcpy(building.shape.data()+28,&off,4);for(int j=32;j<36;++j)building.shape[j]=uint8_t(j);
 for(auto&gun:building.guns){gun.geometry=std::make_shared<const voxel::Model>(model());gun.identity=std::make_shared<const uint8_t>(0);gun.frames=8;}
 building.gunOffset=2.5f;building.gunX=3;building.gunY=-4;building.gunZ=2;
 for(int n=0;n<6;++n){auto q=pose(n);q.gunFrame=1+n%3;q.rawBody=uint16_t(n*9173);assert(c.renderActor(building,q,failed));assert(failed.attachments.size()==2);imageHash(failed);++checked;}
 }
#ifndef GEOMETRY_BASELINE
 // Actual geometry helper: owner identity, mutable bypass, bounded LRU,
 // old published owner lifetime, negative input and failed HVA retry.
 auto g=model();auto id=std::make_shared<const uint8_t>(0);voxelgeometry::Cache probe;auto one=probe.animated(g,id,1,2);assert(one);size_t cost=probe.bytes();assert(cost>g.faces.size()*sizeof(voxel::Face));
 voxelgeometry::Cache cache(cost*2);auto ia=std::make_shared<const uint8_t>(0),ib=std::make_shared<const uint8_t>(0),ic=std::make_shared<const uint8_t>(0);
 auto first=cache.animated(g,ia,1,2),second=cache.animated(g,ib,1,2);assert(first&&second&&cache.entries()==2);assert(cache.animated(g,ia,1,2)==first);auto third=cache.animated(g,ic,1,2);assert(third&&cache.entries()==2&&cache.evictions()==1);assert(cache.animated(g,ia,1,2)==first);assert(cache.animated(g,ib,1,2)!=second);assert(second->faces.size()==g.faces.size()&&cache.bytes()<=cost*2);
 assert(!cache.animated(g,id,-1,1));voxelgeometry::Cache tooSmall(1);assert(tooSmall.animated(g,id,1,2)&&tooSmall.animated(g,id,1,2)&&tooSmall.entries()==0&&tooSmall.bytes()==0&&tooSmall.builds()==2);
 auto manual=cache.animated(g,{},1,2);g.faces[0].p[0].x+=4;auto changed=cache.animated(g,{},1,2);assert(manual!=changed&&manual->faces[0].p[0].x!=changed->faces[0].p[0].x);
 auto invalid=g;invalid.faces[0].normal={0,0,0};auto bad=std::make_shared<const uint8_t>(0);assert(!cache.animated(invalid,bad,1,2));assert(!cache.animated(invalid,bad,1,2));
 voxelgeometry::Cache traits;assert(traits.indexed(g,id));auto scans=traits.faceChecks();for(int n=0;n<100;++n)assert(traits.indexed(g,id));assert(traits.faceChecks()==scans&&scans==g.faces.size());
 g.faces[0].hasNormal=false;g.faces[0].normalIndex=0;auto other=std::make_shared<const uint8_t>(0);assert(!traits.indexed(g,other));assert(!traits.indexed(g,{}));g.faces[0].hasNormal=true;assert(traits.indexed(g,{}));
 cache.clear();assert(!cache.bytes()&&!cache.entries());
#endif
 // Dynamic production Context timing, 96 different angles per batch but
 // repeated immutable HVA frames. Light/palette refresh is outside this clock
 // in both versions; downstream raster/preparation is included equally.
 std::vector<double>cpu,gpu;for(bool direct:{false,true})for(int trial=0;trial<5;++trial){Reader r;Context assets(r);assets.identity(200+trial,1);assets.presentationGPU(direct);assets.beginPresentation();auto object=art();double start=now();
  for(int n=0;n<96;++n){voxel::Image im;assert(assets.renderActor(object,pose(n),im));consumed+=im.gpuRaster?im.gpuRaster->triangles.size():im.pixels.size();}
  double elapsed=now()-start;(direct?gpu:cpu).push_back(elapsed);
#ifndef GEOMETRY_BASELINE
  assert(assets.voxelGeometryBuilds()==6&&assets.voxelGeometryHits()>=3&&assets.voxelGeometryCacheBytes()<=rendermemory::PacketBytes/4);
#endif
 }
 printf("{\"status\":\"PASS\",\"digest\":\"%016llx\",\"cases\":%u,\"faces\":%zu,\"poses_per_batch\":96,\"cpu_body_shadow_ms\":%.6f,\"gpu_packet_prepare_ms\":%.6f,\"scope\":\"actual Context kernels; changing poses; no GPU execution or game FPS\"}\n",(unsigned long long)digest,checked,a.model.faces.size(),median(cpu),median(gpu));
}
