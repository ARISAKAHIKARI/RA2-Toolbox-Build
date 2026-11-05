#pragma once
#include "radar_rank.h"
#include "game_view.h"
#include "display_state.h"
#include "visual_pose.h"
#include "building_animations.h"
#include "map_bounds.h"
#include "art_variants.h"
#include "repair_visuals.h"
#include "projectile_visuals.h"
#include "native_feedback.h"
#include "particle_visuals.h"
#include "voxel_animation_visuals.h"
#include "world_effects.h"
#include "native_draw_state.h"
#include "native_scene_extras.h"
#include "tether_draw_order.h"
#include "native_display_order.h"
#include "../../shared/source/gpu_surface.h"
#include "radar_shroud_selector.h"
#include <memory>
#include <array>
#include "../../shared/source/parallel_rows.h"
// Read-only YRpp MapClass/CellClass/RadarClass model. Projection matches
// GetCrdOnRadar (0x6557F0); never calls game functions or captures the screen.
namespace external { namespace gpumotion {struct Guides;} namespace radar {
constexpr uint32_t Map=0x87F7E8;
struct Geometry {
 int width=0,height=0,offsetX=0,offsetY=0,rw=0,rh=0;float factor=0;uint32_t cells=0,house=0;int count=0;mapbounds::Bounds bounds;int rawWidth=0,rawHeight=0;
 // SetVisibleRect changes the active radar span without necessarily
 // reallocating its RGB backing. Keep that source identity for fresh reads;
 // rawWidth/Height and offsetX/Y describe the private presentation instead.
 bool projectionAdjusted=false;int backingWidth=0,backingHeight=0,backingOffsetX=0,backingOffsetY=0;
 int sourceWidth()const{return projectionAdjusted?backingWidth:rawWidth;}
 int sourceHeight()const{return projectionAdjusted?backingHeight:rawHeight;}
 int sourceOffsetX()const{return projectionAdjusted?backingOffsetX:offsetX;}
 int sourceOffsetY()const{return projectionAdjusted?backingOffsetY:offsetY;}
 bool valid()const{return bounds.valid()&&bounds.width==width&&bounds.height==height&&cells>=0x10000&&count>0&&count<=512*512&&rw>0&&rw<=4096&&rh>0&&rh<=4096&&rawWidth>0&&rawWidth<=4096&&rawHeight>0&&rawHeight<=4096&&std::isfinite(factor)&&factor>0.01f&&factor<32&&std::abs(int64_t(offsetX))<=1024&&std::abs(int64_t(offsetY))<=1024&&house>=0x10000;}
 bool same(const Geometry&o)const{return width==o.width&&height==o.height&&offsetX==o.offsetX&&offsetY==o.offsetY&&rw==o.rw&&rh==o.rh&&rawWidth==o.rawWidth&&rawHeight==o.rawHeight&&factor==o.factor&&cells==o.cells&&house==o.house&&count==o.count&&bounds==o.bounds&&sourceWidth()==o.sourceWidth()&&sourceHeight()==o.sourceHeight()&&sourceOffsetX()==o.sourceOffsetX()&&sourceOffsetY()==o.sourceOffsetY();}
 bool legal(int x,int y)const{return bounds.legal(x,y);}
 // RadarClass::FillTerrainColors clips a flat 2x1 cell footprint against its
 // integer RGB backing. Level affects TMP pixels later, not cell admission.
 bool flatVisible(int x,int y)const{int64_t left=int64_t(x)-y+offsetX,top=int64_t(x)+y-offsetY;
  return left<rawWidth&&left+2>0&&top>=0&&top<rawHeight;}
 // The RGB radar footprint is not a TMP sprite's source rectangle. Keep a
 // bounded source neighborhood, then let NativeCanvas clip the decoded art
 // to the unchanged raw viewport. terrain::tile accepts at most 512x512,
 // including its extra image, and the signed Level may shift it vertically.
 bool terrainSourceCandidate(int x,int y)const{
  const int64_t px=(int64_t(x)-y+offsetX)*30-30,py=(int64_t(x)+y-offsetY)*15;
  return px<rawWidth*30ll+512&&px+512>0&&py-127*15<rawHeight*15ll+512&&py+128*15+512>0;
 }
 bool terrainSourceVisible(int x,int y,int level)const{
  mapbounds::TileOrigin at;if(!tileOrigin(x,y,level,at))return false;
  return at.x<rawWidth*30ll+512&&int64_t(at.x)+512>0&&at.y<rawHeight*15ll+512&&int64_t(at.y)+512>0;
 }
 mapbounds::NativeViewport nativeViewport()const{return valid()?mapbounds::NativeViewport{rawWidth*30.,rawHeight*15.}:mapbounds::NativeViewport{};}
 bool tileOrigin(int x,int y,int level,mapbounds::TileOrigin&out)const{if(!bounds.tileOrigin(x,y,level,out))return false;out.x+=offsetX*30;out.y-=offsetY*15;return true;}
 // Cell::DrawShroud places its 60x30 SHP on the flat cell footprint. Raised
 // terrain moves the TMP art, but never the shroud/fog mask.
 bool shroudOrigin(int x,int y,mapbounds::TileOrigin&out)const{return tileOrigin(x,y,0,out);}
 bool shroudSourceVisible(int x,int y)const{
  mapbounds::TileOrigin at;return shroudOrigin(x,y,at)&&at.x<rawWidth*30ll&&int64_t(at.x)+60>0&&
   at.y<rawHeight*15ll&&int64_t(at.y)+30>0;
 }
 void project(double x,double y,double&u,double&v)const{u=(x-y+offsetX)/rawWidth;v=(x+y-offsetY)/rawHeight;}
};
// A complete Tactical overview has different edges from the native Radar
// crop. Use the camera's pixel extent for this panel only; retain the actual
// Radar allocation/offset as the source identity for fresh publication reads.
// This changes neither the game's VisibleRect nor its camera clamp.
inline bool tacticalOverviewProjection(const Geometry&source,Geometry&out){
 if(!source.valid())return false;
 Geometry native=source;native.rawWidth=source.sourceWidth();native.rawHeight=source.sourceHeight();
 native.offsetX=source.sourceOffsetX();native.offsetY=source.sourceOffsetY();native.projectionAdjusted=false;
 if(!native.valid())return false;Geometry next=native;const auto extent=source.bounds.cameraExtent();
 next.offsetX=int(-extent.left/30.);next.offsetY=int(extent.top/15.);
 next.rawWidth=int((extent.right-extent.left)/30.);next.rawHeight=int((extent.bottom-extent.top)/15.);
 next.backingWidth=native.rawWidth;next.backingHeight=native.rawHeight;
 next.backingOffsetX=native.offsetX;next.backingOffsetY=native.offsetY;
 next.projectionAdjusted=next.rawWidth!=next.backingWidth||next.rawHeight!=next.backingHeight||
  next.offsetX!=next.backingOffsetX||next.offsetY!=next.backingOffsetY;
 if(!next.valid())return false;out=next;return true;
}
// Read-only replay of RadarClass::SetVisibleRect's geometric result. Its
// original Cell iterator and mode-1 usable predicate were checked together,
// including terrain Level and top-slope adjustment. This is radar projection,
// not a Tactical pixel mask or a write to the native VisibleRect.
class ActiveProjection {
 Geometry source_,projected_;bool ready_=false;
public:
 void reset(){source_={};projected_={};ready_=false;}
 template<class Read>bool resolve(Read read,const Geometry&source,Geometry&out){
  Geometry input=source;input.rawWidth=source.sourceWidth();input.rawHeight=source.sourceHeight();input.offsetX=source.sourceOffsetX();input.offsetY=source.sourceOffsetY();input.projectionAdjusted=false;
  if(!input.valid())return false;
  if(ready_&&source_.same(input)){out=projected_;return true;}
  std::vector<uint32_t>pointers(size_t(input.count),0);if(!read(input.cells,pointers.data(),pointers.size()*4))return false;
  const int offsetX=input.width-2*input.bounds.left-1;
  int left=10000,right=-10000,top=10000,bottom=-10000;
  for(size_t n=0;n<pointers.size();++n){const int x=int(n%512),y=int(n/512);if(!input.legal(x,y)||!pointers[n])continue;
   const auto p=pointers[n];if(p<0x10000||p>0x7ffffe00u)return false;std::array<uint8_t,0xF9>fields{};
   if(!read(p+0x24,fields.data(),fields.size()))return false;int16_t coordinate[2]{};std::memcpy(coordinate,fields.data(),4);
   if(coordinate[0]!=x||coordinate[1]!=y)return false;
   if(!input.bounds.usable(x,y,int8_t(fields[0xF7]),fields[0xF8]!=0))continue;
   int at=x-y+offsetX,width=2;if(at==-1){at=0;width=1;}else if(at==2*input.bounds.columns-1)width=1;
   left=std::min(left,at);right=std::max(right,at+width);top=std::min(top,x+y);bottom=std::max(bottom,x+y+1);
  }
  if(left>right||top>=bottom||right-left<=0||right-left>4096||bottom-top>4096)return false;
  out=input;out.offsetX=offsetX-left;out.offsetY=top;out.rawWidth=right-left;out.rawHeight=bottom-top;
  if(out.offsetX!=input.offsetX||out.offsetY!=input.offsetY||out.rawWidth!=input.rawWidth||out.rawHeight!=input.rawHeight){
   out.projectionAdjusted=true;out.backingWidth=input.rawWidth;out.backingHeight=input.rawHeight;out.backingOffsetX=input.offsetX;out.backingOffsetY=input.offsetY;
  }
  if(!out.valid())return false;source_=input;projected_=out;ready_=true;return true;
 }
};
struct Marker {double u=0,v=0;uint32_t color=0;bool building=false;uint32_t object=0,id=0,type=0,owner=0,parent=0,parentId=0;int x=0,y=0,z=0,kind=-1,drawZ=0,sort=0,layer=2;bool normalLight=false,onBridge=false,disguised=false,tetherHidden=false;std::vector<tetherdraw::Key>tetherRedraw;int16_t waterline=0;camera::Material material;visual::Pose pose;int displayOrder=-1,rank=-1;};
struct CellArt {uint32_t cell=0,type=0,overlayType=0,overlaySourceType=0,smudgeType=0,rubbleType=0,flags=0,pixelFX=0;int land=0;uint8_t smudgeData=0;int16_t overlay=-1;uint8_t sub=0;int8_t level=0;uint8_t overlayFrame=0;bool sourceHidden=false;uint32_t sourceColor=0;};
struct Scene {int width=0,height=0;std::vector<uint32_t> pixels;std::shared_ptr<const gpumotion::Guides> guides;std::shared_ptr<const gpupresent::Frame> gpu;bool boltsRendered=false;};
// The game stores the selected shroud/fog SHP frame at each Cell. -1 selects
// clear frame 0, -2 selects opaque frame 15; other values select edge frames.
// Preserve those values so the native-sized scene can cover protruding TMP
// pixels at the projected cell position after the terrain is composed.
// 127 marks an absent/invalid physical Cell, distinct from a valid, opaque -2.
struct ShroudFrames {int8_t shroud=127,fog=127;
 bool operator==(const ShroudFrames&o)const{return shroud==o.shroud&&fog==o.fog;}
};
struct Terrain {Geometry geo;std::vector<uint32_t> colors,baseColors;std::vector<CellArt> art;std::vector<ShroudFrames> shade;bool reveal=false;};
struct EffectSnapshot {std::vector<nativefeedback::Bolt> bolts;bool boltsValid=false;std::shared_ptr<const worldfx::Snapshot>world;};
struct Frame {std::shared_ptr<const Terrain> terrain;std::vector<Marker> markers;std::shared_ptr<const Scene> scene,map;uint32_t tick=0;int frame=-1;bool markerScanValid=false,posesPrepared=false;std::vector<repairvisuals::Sprite> repairs;std::shared_ptr<const sceneextras::Snapshot> extras;std::shared_ptr<const EffectSnapshot> effects;uint8_t effectMask=3;double observedMs=0;};
inline bool supportsEffects(const Frame&frame,bool particles,bool lasers){return (!particles||(frame.effectMask&1))&&(!lasers||(frame.effectMask&2));}
inline uint32_t terrainColor(int land,int level,bool fog){
 static constexpr uint32_t colors[]={0x66794b,0x85836c,0x285e86,0x70756d,0x999684,0xb7a843,0xb4a47c,0x77734d,0xb4cbd0,0x77766b,0x55584f,0x58703d};
 if(land<0||land>=12)return 0x44483a;uint32_t c=colors[land];int shade=std::clamp(level,0,14)*2;
 int r=std::min(255,int(c>>16)+shade),g=std::min(255,int((c>>8)&255)+shade),b=std::min(255,int(c&255)+shade);if(fog){r=r*2/3;g=g*2/3;b=b*2/3;}return uint32_t(r<<16|g<<8|b);
}
template<class Read> class Sampler {
 ActiveProjection projection_;
 bool particles_=true,lasers_=true;Read read_;GameView<Read> view_;Geometry geo_{};std::vector<uint32_t> pointers_;std::shared_ptr<Terrain> pending_;std::shared_ptr<const Terrain> terrain_;
 std::vector<uint32_t> tileTypes_,overlayTypes_,smudgeTypes_,prefetchCells_;std::vector<artvariants::Pool> resourcePools_;size_t cursor_=0;int lastFrame_=-1;uint32_t reveal_=0;unsigned revision_=0;
 struct MaskBits {uint8_t state=0,shroud=0,fog=0;}; // state: unknown, verified, unavailable
 std::vector<MaskBits> maskBits_;std::array<int8_t,256> frameLookup_{};bool pendingIncomplete_=false;
 void discardPending(){pending_.reset();pointers_.clear();maskBits_.clear();cursor_=0;pendingIncomplete_=false;}
 template<class T>T get(uint32_t a,T fallback={})const{return view_.template get<T>(a,fallback);}
 template<class R>static auto prefetch(R read,const std::vector<uint32_t>&p,size_t span,int)->decltype(read.prefetch(p,span),void()){read.prefetch(p,span);}
 template<class R>static void prefetch(R,const std::vector<uint32_t>&,size_t,long){}
 bool maskBits(int x,int y,shroudselector::Bits&out){
  if(!geo_.legal(x,y))return false;size_t i=size_t(y)*512+x;
  if(i>=pointers_.size())return false;
  auto&bits=maskBits_[i];if(!bits.state){
   bits.state=2;uint32_t p=pointers_[i];if(p>=0x10000){
    // One page-cached read replaces three remote field calls for a neighbor.
    std::array<uint8_t,0x11D> field{};
    if(read_(p+0x24,field.data(),field.size())){
     int16_t xy[2]{};std::memcpy(xy,field.data(),sizeof xy);
     if(xy[0]==x&&xy[1]==y)bits={1,field[0x108],field[0x11C]};
     else pendingIncomplete_=true;
    }else pendingIncomplete_=true;
   }else if(p)pendingIncomplete_=true;
  }
  if(bits.state!=1)return false;out={bits.shroud,bits.fog};return true;
 }
 int8_t maskFrame(int x,int y,bool fog){
  // Tactical's cached +0x120/+0x121 can be stale offscreen; the selector
  // reads the same live neighbor bits through one cached accessor per cell.
  return shroudselector::frame(x,y,fog,frameLookup_,[&](int xx,int yy,shroudselector::Bits&out){return maskBits(xx,yy,out);});
 }
public:
 explicit Sampler(Read r):read_(r),view_(r){}
 void effects(bool particles,bool lasers){particles_=particles;lasers_=lasers;}
 void reset(bool keepProjection=false){if(!keepProjection)projection_.reset();geo_={};discardPending();terrain_.reset();lastFrame_=-1;++revision_;}
 bool radarProjectionCompatible(const Frame&frame,bool tacticalOverview=false){Geometry projected;return frame.terrain&&
  (tacticalOverview?tacticalOverviewProjection(frame.terrain->geo,projected):projection_.resolve(read_,frame.terrain->geo,projected))&&projected.same(frame.terrain->geo);}
 unsigned revision()const{return revision_;}
 bool hasTerrain()const{return bool(terrain_);}
 Frame terrainFrame(uint32_t now)const{Frame out;out.terrain=terrain_;out.tick=now;out.frame=lastFrame_;return out;}
 void useTerrain(const Frame&frame){
  if(!frame.terrain)return;
  if(!geo_.same(frame.terrain->geo)||reveal_!=uint32_t(frame.terrain->reveal))reset();
  terrain_=frame.terrain;geo_=terrain_->geo;reveal_=terrain_->reveal;lastFrame_=frame.frame;
 }
 void seedTerrain(const Frame&frame){
  if(terrain_||!frame.terrain||frame.terrain->art.empty())return;
  reset();terrain_=frame.terrain;geo_=terrain_->geo;reveal_=terrain_->reveal;lastFrame_=frame.frame;++revision_;
 }
 bool step(uint32_t reveal,int budget=4096,bool real=false,bool compactTraversal=false,bool activeRadarProjection=false,bool tacticalOverview=false){
  // These are preview pixels, not the game's half-opacity shroud blitter.
  // Every display override exposes the unmasked minimap; native mode alone
  // follows the explored/fog cells. Keep this policy in the immutable bank.
  reveal=reveal?1u:0u;
  Geometry g;g.bounds=mapbounds::sample(read_);g.width=g.bounds.width;g.height=g.bounds.height;g.cells=get<uint32_t>(Map+0x13C);g.count=get<int>(Map+0x140);
  g.factor=get<float>(Map+0x1488);g.rw=get<int>(Map+0x14A4);g.rh=get<int>(Map+0x14A8);g.rawWidth=get<int>(Map+0x1240,-1);g.rawHeight=get<int>(Map+0x1244,-1);g.house=get<uint32_t>(0xA83D4C);
  // Zero is a valid projection offset, so a failed read cannot use it as a fallback.
  if(!read_(Map+0x1490,&g.offsetX,sizeof g.offsetX)||!read_(Map+0x1498,&g.offsetY,sizeof g.offsetY)){discardPending();return false;}
  int frame=get<int>(0xA8ED84,-1);if(!g.valid()||frame<0||get<uint8_t>(0xA8E9A0)!=1 ){discardPending();return false;}
  if(!display::inspect(read_).visible())return false;
  if(tacticalOverview){Geometry active;if(!tacticalOverviewProjection(g,active)){discardPending();return false;}g=active;}
  else if(activeRadarProjection){Geometry active;if(!projection_.resolve(read_,g,active)){discardPending();return false;}g=active;}
  if(!g.same(geo_)||frame<lastFrame_||reveal_!=reveal){reset(activeRadarProjection);geo_=g;reveal_=reveal;}lastFrame_=frame;
  if(!pending_){pointers_.resize(g.count);if(!read_(g.cells,pointers_.data(),pointers_.size()*4)||(!reveal&&!shroudselector::readLookup(read_,frameLookup_))){discardPending();return false;}
   if(!reveal)maskBits_.resize(512*512);
   pending_=std::make_shared<Terrain>();pending_->geo=g;pending_->reveal=bool(reveal);pending_->colors.resize(512*512);pending_->shade.resize(512*512);if(real){
    // An unreadable catalogue is not an empty one. Otherwise live cells lose
    // their materials in a supposedly complete published terrain bank.
    if(!view_.vecChecked(0xA8ED28,tileTypes_,65536)||!view_.vecChecked(0xA83D80,overlayTypes_)||!view_.vecChecked(0xA8EC18,smudgeTypes_,4096)){discardPending();return false;}
    pending_->baseColors.resize(512*512);pending_->art.resize(512*512);
    auto checkedArt=[&](uint32_t at,void*out,size_t bytes){bool ok=read_(at,out,bytes);if(!ok)pendingIncomplete_=true;return ok;};resourcePools_=artvariants::pools(checkedArt,overlayTypes_);
    if(pendingIncomplete_){discardPending();return false;}
   }cursor_=0;}
  const size_t limit=size_t(std::clamp(budget,1,16384));
  size_t end=std::min(pointers_.size(),cursor_+limit);
  if(compactTraversal){
   // The Cell pointer array is a fixed 512x512 address grid, much larger
   // than a normal map. Empty/illegal slots need no process read and must
   // not consume an entire frame's sampling budget. Bound real Cell work
   // exactly as before, while skipping those holes in the same local pass.
   end=cursor_;size_t admitted=0;
   while(end<pointers_.size()){
    const int x=int(end%512),y=int(end/512);
    if(pointers_[end]&&g.legal(x,y)&&(g.flatVisible(x,y)||(real&&g.terrainSourceCandidate(x,y)))){
     if(admitted==limit)break;++admitted;
    }++end;
   }
  }
  // The same bounded chunk and admission as the scalar pass below. Reading
  // adjacent Cell pages together changes neither scan frequency nor which
  // cells are required; failed batches retain the ordinary checked fallback.
  prefetchCells_.clear();prefetchCells_.reserve(std::min(end-cursor_,limit));
  for(size_t index=cursor_;index<end;++index){int x=int(index%512),y=int(index/512);
   if(pointers_[index]>=0x10000&&g.legal(x,y)&&(g.flatVisible(x,y)||(real&&g.terrainSourceCandidate(x,y))))prefetchCells_.push_back(pointers_[index]);
  }
  prefetch(read_,prefetchCells_,0x148,0);
  // Retain the native RGB/diagram/actor admission separately from terrain
  // sources: an excluded cell's TMP can still intersect the current crop.
  for(;cursor_<end;++cursor_){int x=int(cursor_%512),y=int(cursor_/512);bool flat=g.flatVisible(x,y);
   if(!g.legal(x,y)||(!flat&&(!real||!g.terrainSourceCandidate(x,y))))continue;uint32_t p=pointers_[cursor_];
   if(!p)continue;if(p<0x10000){pendingIncomplete_=true;continue;}
   std::array<uint8_t,0x148> cell{};if(!read_(p,cell.data(),cell.size())){pendingIncomplete_=true;continue;}
   int16_t cx=0,cy=0;int land=-1;std::memcpy(&cx,cell.data()+0x24,2);std::memcpy(&cy,cell.data()+0x26,2);std::memcpy(&land,cell.data()+0xEC,4);
   if(cx!=x||cy!=y){pendingIncomplete_=true;continue;}
   if(!reveal)maskBits_[cursor_]={1,cell[0x12C],cell[0x140]};
   int level=int8_t(cell[0x11B]);if(real&&flat)pending_->baseColors[cursor_]=terrainColor(land,level,false);
   bool admitted=flat&&(!reveal||g.bounds.usable(x,y,level,cell[0x11C]!=0));
   bool terrainSource=real&&g.terrainSourceVisible(x,y,level);
   if(!admitted&&!terrainSource&&(!real||!g.shroudSourceVisible(x,y)))continue;
   int8_t shroud=reveal?-1:maskFrame(x,y,false),fog=reveal?-1:maskFrame(x,y,true);
   if(shroud<-2||shroud>46||fog<-2||fog>46){pendingIncomplete_=true;continue;}
   pending_->shade[cursor_]={shroud,fog};
   // A partial shroud frame still reveals part of this cell. Keep its source
   // pixels; the composed scene is clipped by the frame's screen-space mask.
   // A raised TMP/extra image can enter a clear destination cell even when
   // its own flat source cell is hidden. Keep TERRAIN source art and let the
   // final destination mask decide visibility; diagram/actors/overlays retain
   // their original admission and never acquire that source-only permission.
   const bool sourceVisible=reveal||shroud!=-2;
   if(!sourceVisible&&!terrainSource)continue;
   uint32_t color=terrainColor(land,level,!real&&!reveal&&fog!=-1);
   if(admitted&&sourceVisible)pending_->colors[cursor_]=color;
   if(terrainSource&&!pending_->art.empty()){auto&a=pending_->art[cursor_];a.cell=p;a.sourceColor=color;a.sourceHidden=!sourceVisible;a.sub=cell[0x11A];a.level=int8_t(cell[0x11B]);a.overlayFrame=cell[0x11E];a.smudgeData=cell[0x11F];a.land=land;std::memcpy(&a.pixelFX,cell.data()+0xFC,4);std::memcpy(&a.flags,cell.data()+0x140,4);std::memcpy(&a.rubbleType,cell.data()+0x40,4);
    int smudge=-1,tile=-1,overlay=-1;std::memcpy(&smudge,cell.data()+0x48,4);std::memcpy(&tile,cell.data()+0x38,4);std::memcpy(&overlay,cell.data()+0x44,4);
    auto catalogueEntry=[&](const std::vector<uint32_t>&types,int index,uint32_t&out){if(index<0)return; if(size_t(index)>=types.size()||types[size_t(index)]<0x10000){pendingIncomplete_=true;return;}out=types[size_t(index)];};
    if(admitted)catalogueEntry(smudgeTypes_,smudge,a.smudgeType);
    if(tile==65535){if(!read_(0xAA10B0,&tile,sizeof tile))pendingIncomplete_=true;a.sub=0;}
    auto checkedArt=[&](uint32_t at,void*out,size_t bytes){bool ok=read_(at,out,bytes);if(!ok)pendingIncomplete_=true;return ok;};
    catalogueEntry(tileTypes_,tile,a.type);if(a.type)a.type=artvariants::tile(checkedArt,a.type,a.sub,x,y,a.flags);
    if(admitted){catalogueEntry(overlayTypes_,overlay,a.overlaySourceType);if(a.overlaySourceType){a.overlay=int16_t(overlay);int drawOverlay=artvariants::overlay(checkedArt,overlayTypes_,resourcePools_,overlay,x,y,cell[0x11C]);catalogueEntry(overlayTypes_,drawOverlay,a.overlayType);}}
   }
  }
  // Missing physical cells are valid holes. Failed reads of existing cells
  // are NOT opaque/absent visibility: reject the candidate and retain the
  // previous complete bank, including its shade. Never publish a dark flash.
  if(pendingIncomplete_){discardPending();return false;}
  if(cursor_==pointers_.size()){terrain_=pending_;discardPending();++revision_;}return true;
 }
 Frame snapshot(uint32_t now,int observedFrame=-1)const{
  GameView<Read> live(read_);Frame out;out.effectMask=uint8_t(particles_)|(uint8_t(lasers_)<<1);out.terrain=terrain_;out.tick=now;out.frame=observedFrame>=0?observedFrame:lastFrame_;if(!terrain_)return out;
  std::vector<uint32_t> bodies;out.markerScanValid=view_.vecChecked(0xA8EC78,bodies);
  if(!out.markerScanValid)return out;
  if(!terrain_->art.empty()){
   auto extras=std::make_shared<sceneextras::Snapshot>(sceneextras::capture(read_,geo_.house,out.frame,!terrain_->reveal,particles_));
   if(particles_)for(size_t n=0;n<terrain_->art.size()&&extras->sparkles.size()<8192;++n){const auto&c=terrain_->art[n];if(!c.pixelFX||!c.cell)continue;
    // DrawPixelFX6D7A30: no occupant, no bridge deck, visible native Cell.
    if(get<uint32_t>(c.cell+0xE4)||(get<uint32_t>(c.cell+0x140)&0x1000)||(!terrain_->reveal&&(!(get<uint8_t>(c.cell+0x12C)&0x10)||terrain_->shade[n].fog!=-1)))continue;
    std::array<int32_t,15>d{},after{};if(!read_(c.pixelFX,d.data(),60)||uint32_t(d[0])!=0x7EFDA4||d[14]>0||d[4]<0||d[4]>8192||d[12]<-31||d[12]>32||d[13]<-15||d[13]>16)continue;
    bool valid=true;for(int k=6;k<12;++k)valid&=d[k]>=0&&d[k]<=255;if(!valid||!read_(c.pixelFX,after.data(),60)||after!=d||get<uint32_t>(c.cell+0xFC)!=c.pixelFX)continue;
    sceneextras::Sparkle s;s.x=int(n%512);s.y=int(n/512);s.level=c.level;s.dx=d[12];s.dy=d[13];s.phase=d[4];s.base=uint32_t(d[9]<<16|d[10]<<8|d[11]);s.peak=uint32_t(d[6]<<16|d[7]<<8|d[8]);extras->sparkles.push_back(s);
   }out.extras=std::move(extras);
  }
  if(!out.extras)out.extras=std::make_shared<const sceneextras::Snapshot>(sceneextras::capture(read_,geo_.house,out.frame,false,false));
  prefetch(read_,bodies,0x800,0);out.markers.reserve(bodies.size());
  for(uint32_t p:bodies){int kind=view_.kind(p);if(!nativedraw::alive(view_,p,kind)||(kind!=3&&get<uint32_t>(p+0x11C)))continue;
   if(kind==3&&!terrain_->reveal&&out.extras&&out.extras->frozenValid&&get<uint8_t>(p+0x6E7))continue;
   auto drawState=nativedraw::capture(read_,p,view_.type(p,kind),kind,geo_.house,out.frame);if(!drawState.visible)continue;
   int x=get<int>(p+0x9C,-1),y=get<int>(p+0xA0,-1);if(x<0||y<0||x>=512*256||y>=512*256)continue;int cx=x/256,cy=y/256;
   if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;uint32_t owner=get<uint32_t>(p+0x21C);uint8_t rgb[3]{};if(owner<0x10000||!read_(owner+22265,rgb,3))continue;
   Marker m;geo_.project(double(x)/256,double(y)/256,m.u,m.v);if(m.u<0||m.u>=1||m.v<0||m.v>=1)continue;
   m.color=uint32_t(rgb[0])<<16|uint32_t(rgb[1])<<8|rgb[2];m.building=kind==3;m.object=p;m.id=get<uint32_t>(p+0x10);m.type=view_.type(p,kind);m.owner=owner;m.x=x;m.y=y;m.z=get<int>(p+0xA4);m.kind=kind;m.onBridge=get<uint8_t>(p+0x8C)!=0;m.pose=visual::pose(live,p,m.type,kind,out.frame);
   m.material=drawState.material;m.waterline=drawState.waterline;m.disguised=drawState.disguised;
   // Native 70A990 uses raw Veterancy and hides the insignia of an enemy
   // disguise. A failed optional read is unknown, never a fabricated rank.
   m.rank=captureRank(read_,p,owner,geo_.house);
   if(drawState.disguised){m.type=drawState.type;m.owner=drawState.owner;m.kind=drawState.kind;m.pose={};if(m.kind<3){m.pose=visual::pose(live,p,m.type,kind,out.frame);if(m.kind!=kind){m.pose.frame=visual::previewFrame(live,m.type,m.kind,int(m.pose.body)-8);m.pose.variant=-1;m.pose.turretFrame=-1;}}}
   m.layer=std::clamp(get<int>(p+0x94,2),0,4);if(kind==3)m.sort=(get<uint8_t>(m.type+0x16C5)?32:0)-(get<uint8_t>(m.type+0x16B7)?16:0);out.markers.push_back(m);
  }
  tetherdraw::capture(read_,out.markers);
  std::shared_ptr<worldfx::Snapshot>world;
  if(!terrain_->art.empty()){
   world=std::make_shared<worldfx::Snapshot>(worldfx::capture(read_,particles_,lasers_));
   auto effects=std::make_shared<EffectSnapshot>();effects->boltsValid=!lasers_||nativefeedback::bolts(read_,effects->bolts);effects->world=world;out.effects=std::move(effects);
   const auto bodies=out.markers;for(const auto&body:bodies)if(body.kind==3)for(const auto&a:animations::attached(read_,body.object,body.id)){
    Marker m;m.object=a.object;m.id=a.id;m.type=a.type;m.owner=a.owner;m.parent=a.parent;m.parentId=a.parentId;m.x=a.x;m.y=a.y;m.z=a.z;m.kind=5;m.pose.frame=a.frame;m.pose.variant=a.blend;m.drawZ=a.zAdjust;m.sort=a.sort;m.layer=a.layer;m.normalLight=a.normalLight;m.onBridge=get<uint8_t>(a.parent+0x8C)!=0;geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
   std::set<uint32_t> seen;for(const auto&m:out.markers)if(m.kind==5)seen.insert(m.object);
   if(particles_)for(const auto&a:animations::all(read_)){if(!seen.insert(a.object).second||!terrain_->colors[(a.y/256)*512+a.x/256])continue;
    Marker m;m.object=a.object;m.id=a.id;m.type=a.type;m.owner=a.owner;m.x=a.x;m.y=a.y;m.z=a.z;m.kind=5;m.pose.frame=a.frame;m.pose.variant=a.blend;m.drawZ=a.zAdjust;m.sort=a.sort;m.layer=a.layer;m.normalLight=a.normalLight;geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
   for(auto p:view_.vec(0xA8E988)){if(get<uint32_t>(p)!=0x7F522C||get<uint8_t>(p+0xCD)||get<int>(p+0x6C)<=0)continue;
    Marker m;m.object=p;m.id=get<uint32_t>(p+0x10);m.type=get<uint32_t>(p+0xC8);m.x=get<int>(p+0x9C,-1);m.y=get<int>(p+0xA0,-1);m.z=get<int>(p+0xA4);m.kind=4;
    m.pose.frame=get<uint8_t>(m.type+0x2B3)?get<int>(p+0xAC):int(get<int>(p+0x6C)<2);
    if(m.x<0||m.y<0||m.x>=512*256||m.y>=512*256||!terrain_->colors[(m.y/256)*512+m.x/256])continue;
    geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
  }
  if(!terrain_->art.empty()){
   if(particles_)for(const auto&s:particles::all(read_)){
    int cx=s.x/256,cy=s.y/256;if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;
    if(s.point){if(world)world->points.push_back({{s.x,s.y,s.z},s.color});continue;}
    Marker m;m.object=s.object;m.id=s.id;m.type=s.type;m.x=s.x;m.y=s.y;m.z=s.z;m.kind=particles::Kind;m.pose.frame=s.frame;m.pose.variant=s.blend;m.layer=s.layer;
    geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
   if(particles_)for(const auto&s:voxelanims::all(read_)){int cx=s.x/256,cy=s.y/256;if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;Marker m;m.object=s.object;m.id=s.id;m.type=s.type;m.owner=s.owner;m.x=s.x;m.y=s.y;m.z=s.z;m.kind=voxelanims::Kind;m.pose=s.pose;m.drawZ=-13;m.sort=0;m.layer=s.layer;geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);out.markers.push_back(m);}
   for(const auto&s:projectiles::all(read_)){
    int cx=s.x/256,cy=s.y/256;if(!geo_.legal(cx,cy)||!terrain_->colors[cy*512+cx])continue;
    Marker m;m.object=s.object;m.id=s.id;m.type=s.type;m.owner=s.owner;m.x=s.x;m.y=s.y;m.z=s.z;m.kind=projectiles::Kind;m.pose=s.pose;m.drawZ=s.drawZ;m.sort=s.sort;m.layer=s.layer;m.onBridge=s.onBridge;
    geo_.project(double(m.x)/256,double(m.y)/256,m.u,m.v);if(m.u>=0&&m.u<1&&m.v>=0&&m.v<1)out.markers.push_back(m);
   }
  }
  nativedisplay::capture(read_,out.markers,out.frame);
  return out;
 }
};
inline uint32_t smoothColor(const Terrain&t,double x,double y){
 const auto&g=t.geo;int cx=int(std::floor(x)),cy=int(std::floor(y));
 if(!g.legal(cx,cy))return 0x101710;
 uint32_t center=t.colors[cy*512+cx];if(!center)return 0x080c08;
 x-=.5;y-=.5;int ix=int(std::floor(x)),iy=int(std::floor(y));double fx=x-ix,fy=y-iy;
 double red=0,green=0,blue=0;
 for(int j=0;j<2;++j)for(int i=0;i<2;++i){double weight=(i?fx:1-fx)*(j?fy:1-fy);int xx=ix+i,yy=iy+j;
  uint32_t c=g.legal(xx,yy)?t.colors[yy*512+xx]:0;if(!c)c=center;
  red+=((c>>16)&255)*weight;green+=((c>>8)&255)*weight;blue+=(c&255)*weight;
 }
 return (uint32_t(red+.5)<<16)|(uint32_t(green+.5)<<8)|uint32_t(blue+.5);
}
inline void raster(const Terrain&t,int w,int h,std::vector<uint32_t>&pixels){
 if(w<=0||h<=0||w>4096||h>4096){pixels.clear();return;}pixels.assign(size_t(w)*h,0x101710);if(!t.geo.valid()||t.colors.size()!=512*512)return;const auto&g=t.geo;
 parallelrows::rows(size_t(h),size_t(w)*h,[&](size_t first,size_t last){for(int py=int(first);py<int(last);++py)for(int px=0;px<w;++px){double u=(px+.5)*g.rawWidth/w-g.offsetX,v=(py+.5)*g.rawHeight/h+g.offsetY;
  pixels[size_t(py)*w+px]=smoothColor(t,(u+v)/2,(v-u)/2);
 }});
}
// Immutable terrain banks change independently of marker/display frames.
// Retain their scaled background so higher refresh rates do not rescale it.
class DiagramRenderer {
 std::shared_ptr<const Terrain> terrain_;std::shared_ptr<const Scene> scene_;
public:
 void reset(){terrain_.reset();scene_.reset();}
 std::shared_ptr<const Scene> render(const std::shared_ptr<const Terrain>&terrain,int width,int height){
  if(!terrain||width<1||height<1||width>2048||height>2048)return {};
  if(terrain_&&scene_&&scene_->width==width&&scene_->height==height&&
     (terrain_==terrain||(terrain_->reveal==terrain->reveal&&terrain_->geo.same(terrain->geo)&&terrain_->colors==terrain->colors))){
   // The sampler completes a fresh immutable bank even when its visible RGB
   // did not change. Compare the original cells, not millions of resampled
   // output pixels; release the prior bank while retaining the same image.
   terrain_=terrain;return scene_;
  }
  auto scene=std::make_shared<Scene>();scene->width=width;scene->height=height;raster(*terrain,width,height,scene->pixels);
  terrain_=terrain;scene_=std::move(scene);return scene_;
 }
};
}}

