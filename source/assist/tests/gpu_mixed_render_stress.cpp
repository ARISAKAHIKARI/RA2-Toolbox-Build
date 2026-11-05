#ifdef NDEBUG
#undef NDEBUG
#endif
#include "mixed_render_workload.h"
#include "../../external_reader/source/vulkan_area.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace external;
namespace {
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point a){return std::chrono::duration<double,std::milli>(Clock::now()-a).count();}
void require(bool condition,const std::string&what){if(!condition)throw std::runtime_error(what);}
struct Options {bool high=false;unsigned frames=300,warmup=20;int width=1280,height=720,areaWidth=512,areaHeight=256;std::string path="both",json,preview;};
unsigned number(const char*s){char*end=nullptr;auto n=std::strtoul(s,&end,10);require(end&&!*end&&n>0&&n<=10000,"invalid positive count");return unsigned(n);}
Options options(int argc,char**argv){Options o;for(int n=1;n<argc;++n){std::string a=argv[n];auto next=[&](){require(n+1<argc,"missing value for "+a);return argv[++n];};
 if(a=="--frames")o.frames=number(next());else if(a=="--warmup")o.warmup=number(next());else if(a=="--tier"){std::string t=next();require(t=="medium"||t=="high","tier must be medium/high");o.high=t=="high";}
 else if(a=="--path")o.path=next();else if(a=="--json")o.json=next();else if(a=="--preview")o.preview=next();
 else if(a=="--width")o.width=int(number(next()));else if(a=="--height")o.height=int(number(next()));else if(a=="--area-width")o.areaWidth=int(number(next()));else if(a=="--area-height")o.areaHeight=int(number(next()));
 else if(a=="--require"){}else if(a=="--help"){std::cout<<"gpu_mixed_render_stress --tier medium|high --frames 300 --warmup 20 --path viewport|area|both --json report.json [--preview prefix]\n";std::exit(0);}else throw std::runtime_error("unknown argument "+a);
 }require(o.path=="both"||o.path=="viewport"||o.path=="area","invalid path");require(o.width<=4096&&o.height<=4096&&o.areaWidth<=2048&&o.areaHeight<=2048,"output too large");return o;}
std::string quoted(const std::string&s){std::ostringstream o;o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<char(c);else if(c=='\n')o<<"\\n";else if(c<32)o<<"?";else o<<char(c);}o<<'"';return o.str();}
uint64_t fingerprint(const std::vector<uint32_t>&p){uint64_t h=1469598103934665603ull;const size_t step=std::max<size_t>(1,p.size()/2048);for(size_t n=0;n<p.size();n+=step){h^=p[n];h*=1099511628211ull;}return h;}
void ppm(const std::string&path,const std::vector<uint32_t>&pixels,int w,int h){if(path.empty())return;std::ofstream f(path,std::ios::binary);require(bool(f),"preview open failed");f<<"P6\n"<<w<<' '<<h<<"\n255\n";for(auto p:pixels){char c[3]={char(p>>16),char(p>>8),char(p)};f.write(c,3);}require(bool(f),"preview write failed");}
struct Sample {double update=0,plan=0,render=0,total=0,prepare=0,wait=0,readback=0;uint64_t uploads=0,uploadedBytes=0,dispatches=0,voxelRasters=0,prefixBuilds=0,readbacks=0,readbackBytes=0,draws=0,effects=0,globalLightPhase=0,globalTerrainInvalidation=0;};
struct Result {std::string name,device;bool software=false;int width=0,height=0;double coldFirst=0,warmup=0,loop=0;uint64_t qualitySamples=0,qualityWords=0;std::set<uint64_t>hashes;std::vector<Sample>samples;};
double value(const Sample&s,unsigned n){switch(n){case 0:return s.update;case 1:return s.plan;case 2:return s.render;case 3:return s.total;case 4:return s.prepare;case 5:return s.wait;default:return s.readback;}}
std::string summary(const Result&r,unsigned column){std::vector<double>v;double sum=0;for(const auto&s:r.samples){double x=value(s,column);v.push_back(x);sum+=x;}std::sort(v.begin(),v.end());auto q=[&](double p){return v[std::max<size_t>(1,size_t(std::ceil(p*v.size())))-1];};double median=v[v.size()/2];if(v.size()%2==0)median=(median+v[v.size()/2-1])/2;std::ostringstream o;o<<std::fixed<<std::setprecision(6)<<"{\"mean\":"<<sum/v.size()<<",\"median\":"<<median<<",\"p95\":"<<q(.95)<<",\"p99\":"<<q(.99)<<",\"max\":"<<v.back()<<'}';return o.str();}
std::string resultJson(const Result&r){std::ostringstream o;o<<std::fixed<<std::setprecision(6);o<<"{\"path\":"<<quoted(r.name)<<",\"device\":"<<quoted(r.device)<<",\"software_device\":"<<(r.software?"true":"false")<<",\"output_width\":"<<r.width<<",\"output_height\":"<<r.height<<",\"measured_frames\":"<<r.samples.size()<<",\"warmup_total_ms\":"<<r.warmup<<",\"cold_first_frame_ms\":"<<r.coldFirst<<",\"continuous_loop_ms\":"<<r.loop<<",\"headless_loop_hz\":"<<r.samples.size()*1000/r.loop<<",\"distinct_output_fingerprints\":"<<r.hashes.size()<<",\"cold_oracle_samples_after_timed_loop\":"<<r.qualitySamples<<",\"exact_oracle_RGBA_words\":"<<r.qualityWords<<",\"milliseconds\":{";
 const char*names[]={"world_update","CPU_plan","GPU_backend","update_plan_render","GPU_prepare","GPU_fence_wait","GPU_readback_copy"};for(unsigned c=0;c<7;++c){if(c)o<<',';o<<quoted(names[c])<<':'<<summary(r,c);}o<<"},\"samples\":[";
 for(size_t n=0;n<r.samples.size();++n){if(n)o<<',';const auto&s=r.samples[n];o<<"{\"frame\":"<<n<<",\"update_ms\":"<<s.update<<",\"plan_ms\":"<<s.plan<<",\"render_ms\":"<<s.render<<",\"total_ms\":"<<s.total<<",\"GPU_prepare_ms\":"<<s.prepare<<",\"GPU_wait_ms\":"<<s.wait<<",\"readback_ms\":"<<s.readback<<",\"uploads\":"<<s.uploads<<",\"uploaded_bytes\":"<<s.uploadedBytes<<",\"dispatches\":"<<s.dispatches<<",\"voxel_rasters\":"<<s.voxelRasters<<",\"native_prefix_rebuilds\":"<<s.prefixBuilds<<",\"readbacks\":"<<s.readbacks<<",\"readback_bytes\":"<<s.readbackBytes<<",\"scene_draws\":"<<s.draws<<",\"effect_commands\":"<<s.effects<<",\"global_light_phase\":"<<s.globalLightPhase<<",\"global_terrain_invalidation\":"<<s.globalTerrainInvalidation<<'}';}o<<"]}";return o.str();}
void diagnostics(Sample&s,const vulkancompute::Diagnostics&d,size_t pixels){s.prepare=d.prepareMilliseconds;s.wait=d.waitMilliseconds;s.readback=d.readbackMilliseconds;s.uploads=d.uploads;s.uploadedBytes=d.uploadedBytes;s.dispatches=d.dispatches;s.voxelRasters=d.voxelRasters;s.readbacks=d.readbacks;s.readbackBytes=pixels*4;require(d.readbacks==1,"headless benchmark expected one complete RGBA readback");}
bool sampleFrame(unsigned n,unsigned count){return n==0||n==count/2||n+1==count;}
struct ViewPlan {
 camera::GpuSceneRecord record;std::shared_ptr<const void>terrainOwner;std::shared_ptr<const std::vector<uint32_t>>basePixels;std::shared_ptr<const std::vector<float>>baseDepth;
 void build(mixedstress::Workload&w,int width,int height,unsigned frame){const float sx=float(width)/w.width,sy=float(height)/w.height;auto owner=w.base.terrainSourceOwner();bool keep=owner==terrainOwner;
  if(!basePixels){basePixels=std::make_shared<const std::vector<uint32_t>>(size_t(width)*height,0xff080c08);baseDepth=std::make_shared<const std::vector<float>>(size_t(width)*height,-std::numeric_limits<float>::infinity());}
  record.begin(width,height,basePixels,baseDepth,colorprecision::Mode::NativeSteps,{},{},{},keep);record.nativeView(sx,sy,true);
  if(!keep){for(const auto&t:w.base.terrainSources())if(t.image)require(record.append(*t.image,t.image,t.x*sx,t.y*sy,sx,sy,t.base,t.light,0,0,false,{},t.material),"terrain viewport recording failed");record.retainPrefix();terrainOwner=owner;}
  for(const auto&d:w.draws)require(record.append(*d.image,d.image,d.x*sx,d.y*sy,sx,sy,d.base,d.light,d.lift,d.drop,false,{},d.material),"actor viewport recording failed");
  record.effects(w.effects(width,height,frame));require(record.valid(),"viewport record invalid");
 }
};
Result runViewport(mixedstress::Workload&w,const Options&o){Result r;r.name="viewport";r.width=o.width;r.height=o.height;vulkancompute::FrameResolve gpu;ViewPlan plan;std::vector<uint32_t>out;
 struct Quality {vulkancompute::FrameJob plan;std::vector<uint32_t>pixels;};std::vector<Quality>quality;
 auto one=[&](unsigned frame,Sample&s){auto total=Clock::now();const auto beforeLights=w.terrainLightUpdates;w.update(frame);s.globalLightPhase=w.lightPhase;s.globalTerrainInvalidation=w.terrainLightUpdates!=beforeLights;s.update=elapsed(total);auto at=Clock::now();plan.build(w,r.width,r.height,frame);s.plan=elapsed(at);s.draws=plan.record.frame().draws.size();s.effects=plan.record.frame().effects->commands.size();at=Clock::now();if(!gpu.renderFrame(plan.record.frame(),out))throw std::runtime_error("viewport Vulkan render failed: "+gpu.diagnostics().reason);s.render=elapsed(at);s.total=elapsed(total);diagnostics(s,gpu.diagnostics(),out.size());};
 auto warmStart=Clock::now();for(unsigned n=0;n<o.warmup;++n){Sample s;one(n,s);if(n==0)r.coldFirst=s.total;}r.warmup=elapsed(warmStart);r.device=gpu.diagnostics().device;r.software=gpu.diagnostics().softwareDevice;
 auto loop=Clock::now();for(unsigned n=0;n<o.frames;++n){Sample s;one(o.warmup+n,s);r.samples.push_back(s);r.hashes.insert(fingerprint(out));if(sampleFrame(n,o.frames))quality.push_back({plan.record.frame(),out});if((n+1)%30==0)std::cout<<"MIXED_PROGRESS viewport "<<n+1<<'/'<<o.frames<<std::endl;}r.loop=elapsed(loop);
 ppm(o.preview.empty()?"":o.preview+".viewport.ppm",out,r.width,r.height);require(o.frames==1||r.hashes.size()>1,"viewport animation output never changed");
 for(const auto&q:quality){vulkancompute::FrameResolve cold;std::vector<uint32_t>expected;if(!cold.renderFrame(q.plan,expected))throw std::runtime_error("viewport cold oracle failed: "+cold.diagnostics().reason);require(expected==q.pixels,"viewport warm/cold mixed scene RGBA differs");++r.qualitySamples;r.qualityWords+=expected.size();}
 return r;
}
Result runArea(mixedstress::Workload&w,const Options&o){Result r;r.name="whole_map_area";r.width=o.areaWidth;r.height=o.areaHeight;vulkancompute::AreaResolve gpu;camera::ObjectCanvas recorder;gpuarea::Plan plan;std::vector<uint32_t>out;
 struct Quality {gpuarea::Plan plan;std::vector<uint32_t>pixels;};std::vector<Quality>quality;
 auto one=[&](unsigned frame,Sample&s){auto total=Clock::now();const auto beforeLights=w.terrainLightUpdates;w.update(frame);s.globalLightPhase=w.lightPhase;s.globalTerrainInvalidation=w.terrainLightUpdates!=beforeLights;s.update=elapsed(total);auto at=Clock::now();require(recorder.planGPU(w.base,w.draws,r.width,r.height,w.width,w.height,plan),"whole-map GPU recording failed");plan.effects=w.effects(r.width,r.height,frame);s.plan=elapsed(at);s.draws=plan.objects?plan.objects->size():0;s.effects=plan.effects->commands.size();at=Clock::now();if(!gpu.render(plan,out))throw std::runtime_error("whole-map Vulkan render failed: "+gpu.diagnostics().reason);s.render=elapsed(at);s.total=elapsed(total);s.prefixBuilds=gpu.prefixBuilds();diagnostics(s,gpu.diagnostics(),out.size());};
 auto warmStart=Clock::now();for(unsigned n=0;n<o.warmup;++n){Sample s;one(n,s);if(n==0)r.coldFirst=s.total;}r.warmup=elapsed(warmStart);r.device=gpu.diagnostics().device;r.software=gpu.diagnostics().softwareDevice;
 auto loop=Clock::now();for(unsigned n=0;n<o.frames;++n){Sample s;one(o.warmup+n,s);r.samples.push_back(s);r.hashes.insert(fingerprint(out));if(sampleFrame(n,o.frames))quality.push_back({plan,out});if((n+1)%30==0)std::cout<<"MIXED_PROGRESS whole_map_area "<<n+1<<'/'<<o.frames<<std::endl;}r.loop=elapsed(loop);
 ppm(o.preview.empty()?"":o.preview+".area.ppm",out,r.width,r.height);require(o.frames==1||r.hashes.size()>1,"whole-map animation output never changed");
 for(const auto&q:quality){vulkancompute::AreaResolve cold;std::vector<uint32_t>expected;if(!cold.render(q.plan,expected))throw std::runtime_error("whole-map cold oracle failed: "+cold.diagnostics().reason);require(expected==q.pixels,"whole-map warm/cold mixed scene RGBA differs");++r.qualitySamples;r.qualityWords+=expected.size();}
 return r;
}
}
int main(int argc,char**argv){Options o;try{o=options(argc,argv);mixedstress::Workload world(o.high);std::vector<Result>results;
 if(o.path=="viewport"||o.path=="both")results.push_back(runViewport(world,o));if(o.path=="area"||o.path=="both")results.push_back(runArea(world,o));
 std::ostringstream report;report<<std::setprecision(9)<<"{\"schema\":\"ra2.mixed-render-stress.v1\",\"status\":\"pass\",\"tier\":"<<quoted(o.high?"high":"medium")<<",\"seed\":"<<world.seed<<",\"asset_origin\":\"Generated valid TMP/SHP-RLE/VXL-HVA binary fixtures decoded by production functions; no private or game asset files\",\"scope\":\"Headless production Vulkan renderer and CPU planning; includes full RGBA readback, excludes game sampling, windows presentation and display refresh. Software Vulkan results are not physical GPU or game FPS.\",\"percentiles\":\"median average middle pair; P95/P99 nearest rank\",\"quality\":\"Frozen frame snapshots at first/middle/last measured updates; cold GPU oracles run only after the entire timed moving loop\",\"world_width\":"<<world.width<<",\"world_height\":"<<world.height<<",\"terrain_instances\":"<<world.tileCount<<",\"SHP_instances\":"<<world.shpCount<<",\"VXL_instances\":"<<world.vxlCount<<",\"animated_SHP_instances\":"<<world.animationCount<<",\"particle_points_per_frame\":"<<world.particleCount<<",\"beam_segments_per_frame\":"<<world.beamCount<<",\"model_faces_body_turret_barrel\":"<<world.modelFaces<<",\"prepared_triangles_all_cached_poses\":"<<world.preparedTriangles<<",\"decoded_TMP_frames\":"<<world.decodedTmpFrames<<",\"decoded_SHP_RLE_frames\":"<<world.decodedShpFrames<<",\"decoded_VXL_HVA_parts\":"<<world.decodedVxlParts<<",\"asset_setup_ms\":"<<world.assetSetupMs<<",\"warmup_updates_per_path\":"<<o.warmup<<",\"results\":[";
 for(size_t n=0;n<results.size();++n){if(n)report<<',';report<<resultJson(results[n]);}report<<"]}\n";if(!o.json.empty()){std::ofstream f(o.json);require(bool(f),"JSON open failed");f<<report.str();require(bool(f),"JSON write failed");}std::cout<<report.str();return 0;
 }catch(const std::exception&e){std::string error="{\"schema\":\"ra2.mixed-render-stress.v1\",\"status\":\"failure\",\"error\":"+quoted(e.what())+"}\n";if(!o.json.empty()){std::ofstream f(o.json);f<<error;}std::cerr<<error;return 1;}}
