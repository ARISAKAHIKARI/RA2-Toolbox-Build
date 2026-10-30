// Execute the production watch cache with same-frame palette mutations.
// Reuse only the portable reader/graphics boundary from its existing fixture.
#define main watchBaselineMain
#include "watch_host.cpp"
#undef main
struct LiveAssets {
 struct Model {struct Faces {size_t count=0;size_t size()const{return count;}void resize(size_t n){count=n;}}faces;};
 struct ActorArt {Model model;uint32_t id=0,color=0;bool voxel=false;};
 std::string failure;int captures=0,renders=0,passes=0;bool readable=true,terrainReadable=false,liveVoxel=false,voxelRenderOK=true;uint32_t voxelLight=0;
 std::map<uint32_t,uint32_t> colors{{1,0xffff0000},{2,0xff0000ff},{3,0xff00ff00}};
 void identity(uint32_t,uint32_t){}uint64_t revision(){return 1;}
 void beginPresentation(){++passes;}
 bool refreshActorPalette(ActorArt&a,bool&changed){changed=false;if(!readable&&a.id==1)return false;changed=a.color!=colors[a.id];a.color=colors[a.id];return true;}
 bool palette(bool,std::array<uint32_t,256>&out){if(!terrainReadable)return false;out.fill(0xffabcdef);return true;}
 bool tile(uint32_t,unsigned,const std::array<uint32_t,256>&,external::voxel::Image&){return false;}
 bool overlay(uint32_t,uint32_t,int,external::voxel::Image&,bool=false){return false;}
 bool captureActor(uint32_t,uint32_t,uint32_t id,ActorArt&a){++captures;a.id=id;a.color=colors[id];a.voxel=liveVoxel;return true;}
 bool captureAnimation(uint32_t p,uint32_t t,uint32_t id,uint32_t,uint32_t,ActorArt&a){return captureActor(p,t,id,a);}
 bool renderActor(const ActorArt&a,const external::visual::Pose&,external::voxel::Image&i){++renders;if(a.voxel&&!voxelRenderOK)return false;i={};i.width=i.height=1;i.pixels={a.color^voxelLight};return true;}
 static size_t cost(const ActorArt&a){return a.model.faces.size()*sizeof(external::voxel::Face);}
};
struct KnownLightAssets:LiveAssets {
 bool available=true;uint64_t voxelLightingRevision()const{return voxelLight;}
 bool voxelLightingAvailable()const{return available;}
};
int main(){
 LiveAssets assets;watchengine::Renderer<decltype(externalReader),LiveAssets> render(externalReader,assets);
 external::watch::Frame frame;frame.tiles.clear();
 for(uint32_t id:{1u,2u,3u}){external::watch::Actor a;a.target={0x10000+id*0x1000,id};a.type=id==3?0x30000:0x20000;a.owner=0x40000;a.kind=id==3?2:5;frame.actors.push_back(a);}
 render.refreshAssets(frame,1000);assert(assets.captures==3&&assets.renders==3&&render.actorArt.size()==3);
 auto pixel=[&](int i){return render.images.at(render.imageKey(frame.actors[i])).image.pixels[0];};
 assert(pixel(0)==0xffff0000&&pixel(1)==0xff0000ff&&pixel(2)==0xff00ff00);
 render.refreshAssets(frame,1001);assert(assets.captures==3&&assets.renders==3);
 assets.colors[1]=0xffffff00;render.refreshAssets(frame,1002);
 assert(assets.captures==3&&assets.renders==4&&pixel(0)==0xffffff00&&pixel(1)==0xff0000ff&&pixel(2)==0xff00ff00);
 assets.readable=false;render.refreshAssets(frame,1003);assert(!render.images.count(render.imageKey(frame.actors[0])));
 assert(render.images.count(render.imageKey(frame.actors[1]))&&assets.captures==3);
 assets.readable=true;render.refreshAssets(frame,1004);assert(pixel(0)==0xffffff00&&assets.captures==3&&assets.renders==5);
 assert(assets.passes==5&&!render.terrainPaletteEverReady&&!render.terrainPaletteUnavailable);
 assets.terrainReadable=true;render.refreshAssets(frame,1005);assert(render.terrainPaletteEverReady&&!render.terrainPaletteUnavailable);
 assets.terrainReadable=false;render.refreshAssets(frame,1006);assert(render.terrainPaletteUnavailable&&render.tiles.empty());
 auto&view=render.views[0];view.shown=true;Graphics graphics;render.drawView(graphics,view);
 assert(view.raster.color.size()==640*512&&std::all_of(view.raster.color.begin(),view.raster.color.end(),[](uint32_t c){return c==0xff000000;}));
 assets.terrainReadable=true;render.refreshAssets(frame,1007);assert(!render.terrainPaletteUnavailable&&assets.captures==3);

 LiveAssets voxelAssets;voxelAssets.liveVoxel=true;
 watchengine::Renderer<decltype(externalReader),LiveAssets> voxels(externalReader,voxelAssets);
 external::watch::Frame voxelFrame;voxelFrame.tiles.clear();voxelFrame.actors={frame.actors[2]};
 voxels.refreshAssets(voxelFrame,2000);assert(voxelAssets.captures==1&&voxelAssets.renders==1);
 auto vk=voxels.imageKey(voxelFrame.actors[0]);uint32_t original=voxels.images.at(vk).image.pixels[0];
 voxelAssets.voxelLight=0x00101010;voxels.refreshAssets(voxelFrame,2001);
 assert(voxelAssets.captures==1&&voxelAssets.renders==2&&voxels.images.at(vk).image.pixels[0]!=original);
 voxels.refreshAssets(voxelFrame,2002);assert(voxelAssets.captures==1&&voxelAssets.renders==3);
 voxelAssets.voxelRenderOK=false;voxels.refreshAssets(voxelFrame,2003);assert(!voxels.images.count(vk)&&voxels.displayed.empty());
 voxelAssets.voxelRenderOK=true;voxels.refreshAssets(voxelFrame,2004);assert(voxels.images.count(vk)&&voxelAssets.captures==1);
 // Oversized per-frame geometry work is rejected before rasterization;
 // previous colored poses are not retained as a budget fallback.
 voxels.actorArt.begin()->second.art.model.faces.resize(2*1024*1024+1);
 int priorRenders=voxelAssets.renders;voxels.refreshAssets(voxelFrame,2005);
 assert(voxelAssets.renders==priorRenders&&!voxels.images.count(vk)&&voxels.displayed.empty());
 // A verified, unchanged light signature and pose reuse the actual image;
 // a lighting change, unavailable VPL or a new pose cannot reuse it.
 KnownLightAssets stable;stable.liveVoxel=true;
 watchengine::Renderer<decltype(externalReader),KnownLightAssets> cached(externalReader,stable);
 cached.refreshAssets(voxelFrame,3000);assert(stable.renders==1&&stable.captures==1);
 cached.refreshAssets(voxelFrame,3001);assert(stable.renders==1);
 stable.voxelLight=0x00010101;cached.refreshAssets(voxelFrame,3002);assert(stable.renders==2);
 voxelFrame.actors[0].pose.bodyFine=3;cached.refreshAssets(voxelFrame,3003);assert(stable.renders==3);
 auto newKey=cached.imageKey(voxelFrame.actors[0]);stable.available=false;stable.voxelRenderOK=false;
 cached.refreshAssets(voxelFrame,3004);assert(!cached.images.count(newKey)&&cached.displayed.empty());
 stable.available=true;stable.voxelRenderOK=true;cached.refreshAssets(voxelFrame,3005);assert(stable.renders==5&&cached.images.count(newKey));
 cached.refreshAssets(voxelFrame,3006);assert(stable.renders==5);
 std::puts("PASS: watch same-pointer palette changes invalidate only affected colored poses, preserve geometry, isolate per-object animations and hide unreadable colors until recovery");
}
