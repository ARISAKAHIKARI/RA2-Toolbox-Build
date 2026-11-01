#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gpu_voxel_cases.h"
#include <chrono>
#include <cstdio>
using namespace external;
static uint64_t digest=1469598103934665603ull;
static void hash(const void*p,size_t n){auto b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i)digest=(digest^b[i])*1099511628211ull;}
static void imageHash(const voxel::Image&i){for(int v:{i.width,i.height,i.x,i.y,i.depthBottom,i.rasterScale})hash(&v,4);hash(i.pixels.data(),i.pixels.size()*4);hash(i.depth.data(),i.depth.size()*4);hash(i.indices.data(),i.indices.size());}
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
int main(){
 // Golden controls include fallback lighting, all normal modes, invalid/special
 // normal indices, independent turret, terrain posture, 1..4x and depth ties.
 uint64_t pixels=0,triangles=0;for(auto&c:gpuvoxeltest::cases(256)){voxel::Image actual;assert(gpuvoxel::rasterReference(c.prepared,actual));assert(actual.pixels==c.expected.pixels&&actual.depth==c.expected.depth&&actual.indices==c.expected.indices);imageHash(c.expected);hash(c.prepared.triangles.data(),c.prepared.triangles.size()*sizeof(gpuvoxel::Triangle));pixels+=actual.pixels.size();triangles+=c.prepared.triangles.size();}
 auto geometry=model();auto vpl=gpuvoxeltest::vpl();std::array<uint32_t,256>palette{};for(unsigned i=1;i<256;++i)palette[i]=0xff000000u|((i*37%256)<<16)|((i*79%256)<<8)|(i*113%256);
 // Every unit and iteration changes HVA/body/turret/gun/recoil inputs. No
 // SceneRenderer, PacketCache or completed-image cache participates.
 std::vector<double>animation,prepare,raster,shadow;constexpr int Units=32,Frames=3;
 for(int repeat=0;repeat<5;++repeat){double am=0,pm=0,rm=0,sm=0;
  for(int frame=0;frame<Frames;++frame)for(int unit=0;unit<Units;++unit){const int n=frame*Units+unit,scale=1+(unit%3);const double angle=n*.071+.003,turn=n*.113+.017,gun=(n%9-4)*.021;std::array<float,2>recoil{{float(n%5)*.09f,float(n%7)*.05f}};
   voxel::Model posed;double start=now();assert(voxel::animated(geometry,1+n%7,1+(n*3)%7,posed));am+=now()-start;
   voxelposture::State posture;posture.active=n%3!=0;posture.sideways=float(n%7-3)*.009f;posture.forwards=float(n%5-2)*.013f;
   gpuvoxel::Prepared packet;start=now();assert(gpuvoxel::prepare(posed,angle,palette,packet,turn,3.5f,&vpl,255,0,false,scale,&posture,gun,recoil));pm+=now()-start;
   voxel::Image image;start=now();assert(voxel::render(posed,angle,palette,image,turn,3.5f,&vpl,255,0,false,scale,&posture,gun,recoil));rm+=now()-start;
   // An independent scalar packet raster remains an oracle for coverage,
   // strict Z winner and all float depth bits. Excluded from timing above.
   if(!repeat){voxel::Image ref;assert(gpuvoxel::rasterReference(packet,ref));assert(ref.pixels==image.pixels&&ref.indices==image.indices&&ref.depth==image.depth);imageHash(image);hash(packet.triangles.data(),packet.triangles.size()*sizeof(gpuvoxel::Triangle));}
   for(auto&f:posed.faces)for(auto&p:f.p)p.z=0;
   start=now();voxel::Image shade;assert(voxel::render(posed,angle,palette,shade,turn,3.5f,nullptr,255,0,false,scale));sm+=now()-start;if(!repeat)imageHash(shade);
  }animation.push_back(am);prepare.push_back(pm);raster.push_back(rm);shadow.push_back(sm);
 }
 auto median=[](auto&v){std::sort(v.begin(),v.end());return v[v.size()/2];};
 std::printf("{\"status\":\"PASS\",\"digest\":\"%016llx\",\"controls_pixels\":%llu,\"controls_triangles\":%llu,\"faces_per_unit\":%zu,\"units\":%d,\"frames\":%d,\"median_hva_ms\":%.3f,\"median_gpu_prepare_ms\":%.3f,\"median_cpu_raster_ms\":%.3f,\"median_shadow_ms\":%.3f,\"scope\":\"changing poses; CPU production kernels and GPU input preparation; no GPU execution\"}\n",(unsigned long long)digest,(unsigned long long)pixels,(unsigned long long)triangles,geometry.faces.size(),Units,Frames,median(animation),median(prepare),median(raster),median(shadow));
}
