#pragma once
// Post-generation quality guard; does not replace the interpolation algorithm.
// Designed for tiny RGBA cursors and safe to run on existing cached timelines.
#include "motion.h"
#include <algorithm>
#include <cmath>
#include <vector>
namespace quality {
struct Summary {
 int removedPixels=0, correctedFrames=0, suspiciousDrift=0;
 double worstJitter=0;
};
struct Center { double x=0,y=0,weight=0; };
inline Center center(const motion::Image& im,int w,int h){
 Center p;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  int alpha=(im[size_t(y)*w+x]>>24)&255;
  if(alpha<48)continue;
  p.x+=alpha*x;p.y+=alpha*y;p.weight+=alpha;
 }
 if(p.weight){p.x/=p.weight;p.y/=p.weight;}
 return p;
}
inline bool pixelNearby(const motion::Image& im,int w,int h,int x,int y,int radius){
 for(int dy=-radius;dy<=radius;dy++)for(int dx=-radius;dx<=radius;dx++){
  int xx=x+dx,yy=y+dy;
  if(xx>=0&&yy>=0&&xx<w&&yy<h && (im[size_t(yy)*w+xx]>>24)>=40)return true;
 }
 return false;
}
// Rotation center sanity check: does the fitted global transform carry
// the alpha centroid of keyframe A near keyframe B? Report, do not rewrite a
// trusted animation based on centroid alone (local deformation may be real).
inline double rotationEndpointError(const motion::Pair& pair){
 if(!pair.rigid.valid)return -1.0;
 auto a=center(pair.a,pair.w,pair.h),b=center(pair.b,pair.w,pair.h);
 if(!a.weight||!b.weight)return -1.0;
 double x,y;rotation::forward(pair.rigid,pair.w,pair.h,a.x,a.y,1.0,x,y);
 return std::hypot(x-b.x,y-b.y);
}
// Global alpha centroid is only an auxiliary diagnostic; the actual QA follows
// spatially detached *components* through time, rather than isolated pixels.
// A spark originating from a native keyframe is protected even while moving.
struct Component {
 int frame=0;
 int area=0;
 double x=0,y=0;
 std::vector<int> pixels;
};
struct Track {
 int parent=0;
 int start=1000000,end=-1;
 int total=0,largest=0;
 bool nearEndpoint=false;
};
inline int rootOf(std::vector<Track>& tracks,int idx){
 int p=idx;
 while(tracks[p].parent!=p)p=tracks[p].parent;
 while(tracks[idx].parent!=idx){int next=tracks[idx].parent;tracks[idx].parent=p;idx=next;}
 return p;
}
inline void join(std::vector<Track>& tracks,int a,int b){
 a=rootOf(tracks,a);b=rootOf(tracks,b);if(a==b)return;
 if(tracks[a].total<tracks[b].total)std::swap(a,b);
 tracks[b].parent=a;
 tracks[a].start=std::min(tracks[a].start,tracks[b].start);
 tracks[a].end=std::max(tracks[a].end,tracks[b].end);
 tracks[a].total+=tracks[b].total;
 tracks[a].largest=std::max(tracks[a].largest,tracks[b].largest);
 tracks[a].nearEndpoint=tracks[a].nearEndpoint||tracks[b].nearEndpoint;
}
// A small connected component that appears only between native frames is an
// interpolator artifact candidate. Link components through consecutive 1ms
// samples with 8-neighbour motion allowance. Analyse a WHOLE trajectory before
// deleting anything: per-frame cleanup alone produces the exact flicker we seek
// to fix. The check is deliberately conservative about long-lived particles.
inline Summary inspectAndRepair(motion::Timeline& timeline,
                                const motion::Image& first,
                                const motion::Image& last){
 Summary stats;
 const int w=timeline.w,h=timeline.h,n=int(timeline.images.size());
 const size_t pixels=size_t(w)*h;
 if(w<1||h<1||n<3||first.size()!=pixels||last.size()!=pixels)return stats;
 for(const auto& im:timeline.images)if(im.size()!=pixels)return stats;
 const auto originals=timeline.images;
 // At threshold 64, a 2x3 moving fleck is one component, not six unrelated
 // pixels. Keep recognizable detached petals / sprite particles (large >18px).
 const int maxFleck=18;
 const int alphaMin=64;
 std::vector<int> prevLabels(pixels,-1),currLabels(pixels,-1);
 std::vector<unsigned char> visited(pixels,0);
 std::vector<int> queue;queue.reserve(pixels);
 std::vector<Component> components;
 std::vector<Track> tracks;
 for(int f=0;f<n;f++){
  (std::fill)(visited.begin(),visited.end(),0);
  (std::fill)(currLabels.begin(),currLabels.end(),-1);
  const auto& im=originals[f];
  for(int k=0;k<int(pixels);k++){
   if(visited[k]||int(im[size_t(k)]>>24)<alphaMin)continue;
   queue.clear();queue.push_back(k);visited[k]=1;
   for(size_t z=0;z<queue.size();z++){
    int id=queue[z],x=id%w,y=id/w;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
     int xx=x+dx,yy=y+dy;
     if(xx<0||yy<0||xx>=w||yy>=h)continue;
     int p=yy*w+xx;
     if(!visited[p]&&int(im[size_t(p)]>>24)>=alphaMin){visited[p]=1;queue.push_back(p);}
    }
   }
   if(int(queue.size())>maxFleck){
    // Large silhouette pixels cannot be part of a rogue-fleck track.
    for(int id:queue)currLabels[id]=-2;
    continue;
   }
   const int idx=int(components.size());
   Component node;node.frame=f;node.area=int(queue.size());node.pixels=queue;
   double sumX=0,sumY=0;
   for(int id:queue){currLabels[id]=idx;sumX+=id%w;sumY+=id/w;}
   node.x=sumX/node.area;node.y=sumY/node.area;
   components.push_back(std::move(node));
   Track t;t.parent=idx;t.start=f;t.end=f;t.total=int(queue.size());t.largest=int(queue.size());
   t.nearEndpoint=(f<=1||f>=n-2);
   tracks.push_back(t);
   // Trace a whole fleck trajectory at native 1ms spacing. Do NOT attach it
   // to the nearest large silhouette just because they pass close by.
   if(f>0){
    for(int id:queue){
     int x=id%w,y=id/w;
     for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
      int xx=x+dx,yy=y+dy;
      if(xx<0||yy<0||xx>=w||yy>=h)continue;
      int prev=prevLabels[size_t(yy)*w+xx];
      if(prev>=0)join(tracks,idx,prev);
     }
    }
   }
  }
  prevLabels.swap(currLabels);
 }
 // Reject detached trajectories born AND dying inside the native interval.
 // For extra safety, require a short lifetime OR isolated alpha jumps. An
 // actual tiny native-frame feature reaches the first/last sample and stays.
 std::vector<unsigned char> removedTrack(tracks.size(),0);
 const int maxDuration=(std::max)(3,(n*2)/3);
 for(size_t idx=0;idx<tracks.size();idx++){
  int base=rootOf(tracks,int(idx));
  if(removedTrack[size_t(base)]||tracks[base].nearEndpoint)continue;
  const int lifetime=tracks[base].end-tracks[base].start+1;
  if(lifetime>maxDuration)continue; // don't destroy sustained real particles
  if(tracks[base].largest>maxFleck)continue;
  removedTrack[size_t(base)]=1;
 }
 // Rewrite every affected time position from the immutable original samples.
 // This removes the rogue block's entire temporal trajectory; leaving the
 // block visible for 1-2 frames would introduce an artificial flash.
 for(size_t idx=0;idx<components.size();idx++){
  int r=rootOf(tracks,int(idx));if(!removedTrack[size_t(r)])continue;
  const Component& c=components[idx];
  if(c.frame<=0||c.frame>=n-1)continue;
  for(int id:c.pixels){
   // Trim light antialias halo as well. Only if no stronger unrelated sprite
   // feature exists at that location in the endpoint-native samples.
   timeline.images[c.frame][size_t(id)]=0;
   stats.removedPixels++;
  }
 }
 // Remove weaker alpha skirts within 1 pixel of the flagged fleck, but only
 // when those skirts also have no temporal or native-frame support. A genuine
 // adjacent petal must not be shaved off with the rogue component.
 for(size_t idx=0;idx<components.size();idx++){
  int r=rootOf(tracks,int(idx));if(!removedTrack[size_t(r)])continue;
  const Component& c=components[idx];
  if(c.frame<2||c.frame>=n-2)continue;
  const auto& raw=originals[c.frame];
  for(int id:c.pixels){
   int x=id%w,y=id/w;
   for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
    int xx=x+dx,yy=y+dy;
    if(xx<0||yy<0||xx>=w||yy>=h)continue;
    size_t p=size_t(yy)*w+xx;int al=int(raw[p]>>24);
    if(al<=0||al>=alphaMin||!int(timeline.images[c.frame][p]>>24))continue;
    // Smooth low-alpha fringes only if they also have no adjacent-native
    // support and aren't a continuous moving part of the main image.
    if(pixelNearby(first,w,h,xx,yy,1)||pixelNearby(last,w,h,xx,yy,1))continue;
    int support=0;
    for(int step:{-2,-1,1,2}){
     int f=c.frame+step;
     if(f>=0&&f<n&&int(originals[f][p]>>24)>=alphaMin)support++;
    }
    if(support==0){timeline.images[c.frame][p]=0;stats.removedPixels++;}
   }
  }
 }
 // Temporal consistency pass: fix only one-tick alpha impulses and holes.
 // It uses already cleaned adjacent samples. Never interpolate an entire
 // moving edge (which would ghost a rotating/contracting silhouette).
 const auto cleaned=timeline.images;
 for(int f=2;f<n-2;f++){
  int filled=0;
  const auto& prev2=cleaned[f-2];const auto& prev=cleaned[f-1];
  const auto& cur=cleaned[f];const auto& next=cleaned[f+1];const auto& next2=cleaned[f+2];
  for(int k=0;k<int(pixels);k++){
   int alpha=int(cur[size_t(k)]>>24),left=int(prev[size_t(k)]>>24),right=int(next[size_t(k)]>>24);
   int x=k%w,y=k/w;
   if(alpha>=96&&left<24&&right<24){
    if(!pixelNearby(prev,w,h,x,y,1)&&!pixelNearby(next,w,h,x,y,1)&&
       !pixelNearby(first,w,h,x,y,1)&&!pixelNearby(last,w,h,x,y,1)){
     if(timeline.images[f][size_t(k)]){timeline.images[f][size_t(k)]=0;stats.removedPixels++;}
    }
   }else if(alpha<32&&left>=192&&right>=192&&
            int(prev2[size_t(k)]>>24)>=128&&int(next2[size_t(k)]>>24)>=128){
    // A single 1-ms transparent pinhole amid five continuously opaque samples
    // is a rasterization glitch, not genuine shrink/rotation motion.
    int delta=0;
    for(int j=0;j<3;j++)delta+=std::abs(int((prev[size_t(k)]>>(8*j))&255)-int((next[size_t(k)]>>(8*j))&255));
    if(delta<60){
     timeline.images[f][size_t(k)]=((prev[size_t(k)]>>24)>=(next[size_t(k)]>>24)?prev[size_t(k)]:next[size_t(k)]);
     filled++;
    }
   }
  }
  if(filled)stats.correctedFrames++;
 }
 // Native-anchored trajectory review. The first pass deliberately retains a
 // long-lived small island, but that can be wrong: a bad flow match may carry a
 // detached 2x3 pixel patch for most of the interval. Being temporally stable
 // is NOT enough: its position must also have plausible native-frame support.
 // The native masks are moved with the global fit only (not the suspect local
 // flow), so an independently wandering patch cannot prove its own validity.
 // Treat whole component tracks as a unit, never erase only their middle frames.
 {
  struct Part {int time=0,area=0,confirmed=0;std::vector<int> ids;};
  struct MotionTrack {int parent=0, first=100000,last=-1,total=0,confirmed=0;bool nativeAnchor=false;};
  std::vector<Part> parts;
  std::vector<MotionTrack> paths;
  std::vector<int> labelsBefore(pixels,-1),labelsNow(pixels,-1);
  std::vector<unsigned char> checked(pixels,0);
  std::vector<int> stack;stack.reserve(pixels);
  // This transform is estimated from immutable endpoints, not processed frames.
  // An invalid fit falls back to a generous union of native silhouettes.
  const rotation::Fit fit=rotation::detect(first,last,w,h);
  const int maxPartArea=28,opaqueCutoff=72;
  for(int time=0;time<n;time++){
   (std::fill)(checked.begin(),checked.end(),0);
   (std::fill)(labelsNow.begin(),labelsNow.end(),-1);
   double phase=double(time)/double(n-1);
   motion::Image projectedA,projectedB;
   if(fit.valid){
    projectedA=rotation::warp(first,w,h,fit.angle*phase,fit.dx*phase,fit.dy*phase,1.0+(fit.scale-1.0)*phase);
    projectedB=rotation::reverseWarp(last,w,h,fit,phase);
   }
   const auto& nativeA=fit.valid?projectedA:first;
   const auto& nativeB=fit.valid?projectedB:last;
   // Two pixels allow pixel-art edges, modest local residuals, and rounding.
   // Disallow weaker/isolated interpolation splats from claiming provenance.
   auto backed=[&](int x,int y){
    return pixelNearby(nativeA,w,h,x,y,1)||pixelNearby(nativeB,w,h,x,y,1);
   };
   const auto& image=timeline.images[time];
   for(int seed=0;seed<int(pixels);seed++){
    if(checked[size_t(seed)]||int(image[size_t(seed)]>>24)<opaqueCutoff)continue;
    stack.clear();stack.push_back(seed);checked[size_t(seed)]=1;
    for(size_t z=0;z<stack.size();z++){
     int v=stack[z],x=v%w,y=v/w;
     for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
      int xx=x+dx,yy=y+dy;
      if(xx<0||yy<0||xx>=w||yy>=h)continue;
      int t=yy*w+xx;
      if(!checked[size_t(t)]&&int(image[size_t(t)]>>24)>=opaqueCutoff){
       checked[size_t(t)]=1;stack.push_back(t);
      }
     }
    }
    if(int(stack.size())>maxPartArea)continue;
    const int id=int(parts.size());
    Part node;node.time=time;node.area=int(stack.size());node.ids=stack;
    for(int v:stack){
     labelsNow[size_t(v)]=id;
     if(backed(v%w,v/w))node.confirmed++;
    }
    parts.push_back(std::move(node));
    MotionTrack path;path.parent=id;path.first=time;path.last=time;
    path.total=parts.back().area;path.confirmed=parts.back().confirmed;
    path.nativeAnchor=(time==0||time==n-1);
    paths.push_back(path);
    if(time>0){
     for(int v:stack){
      int x=v%w,y=v/w;
      for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
       int xx=x+dx,yy=y+dy;
       if(xx<0||yy<0||xx>=w||yy>=h)continue;
       int other=labelsBefore[size_t(yy)*w+xx];
       if(other<0)continue;
       int a=id,b=other;
       while(paths[size_t(a)].parent!=a)a=paths[size_t(a)].parent;
       while(paths[size_t(b)].parent!=b)b=paths[size_t(b)].parent;
       if(a==b)continue;
       if(paths[size_t(a)].total<paths[size_t(b)].total)std::swap(a,b);
       paths[size_t(b)].parent=a;
       paths[size_t(a)].total+=paths[size_t(b)].total;
       paths[size_t(a)].confirmed+=paths[size_t(b)].confirmed;
       paths[size_t(a)].first=std::min(paths[size_t(a)].first,paths[size_t(b)].first);
       paths[size_t(a)].last=std::max(paths[size_t(a)].last,paths[size_t(b)].last);
       paths[size_t(a)].nativeAnchor=paths[size_t(a)].nativeAnchor||paths[size_t(b)].nativeAnchor;
      }
     }
    }
   }
   labelsBefore.swap(labelsNow);
  }
  std::vector<unsigned char> unsupported(paths.size(),0);
  for(size_t i=0;i<paths.size();i++){
   size_t root=i;
   while(paths[root].parent!=int(root))root=size_t(paths[root].parent);
   auto& path=paths[root];
   // Never remove any path grounded in a real native endpoint. That protects
   // rotating detached spokes and genuinely moving isolated particles.
   if(path.nativeAnchor||path.total==0)continue;
   // A trajectory that is mostly backed by native-frame silhouettes is real,
   // even when it leaves/joins a larger body over time.
   if(path.confirmed*5>=path.total)continue;
   unsupported[root]=1;
  }
  std::vector<unsigned char> flagged(pixels,0);
  for(size_t i=0;i<parts.size();i++){
   size_t root=i;
   while(paths[root].parent!=int(root))root=size_t(paths[root].parent);
   if(!unsupported[root]||parts[i].time<=0||parts[i].time>=n-1)continue;
   int t=parts[i].time;
   (std::fill)(flagged.begin(),flagged.end(),0);
   for(int id:parts[i].ids){
    flagged[size_t(id)]=1;
    if(timeline.images[size_t(t)][size_t(id)]){
     timeline.images[size_t(t)][size_t(id)]=0;
     stats.removedPixels++;
    }
   }
   // Remove only the attached low-opacity fringe of this actual component;
   // never touch neighbouring native-supported body pixels.
   for(int id:parts[i].ids){
    int x=id%w,y=id/w;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
     int xx=x+dx,yy=y+dy;
     if(xx<0||yy<0||xx>=w||yy>=h)continue;
     size_t q=size_t(yy)*w+xx;
     if(flagged[q]||int(timeline.images[size_t(t)][q]>>24)>=opaqueCutoff)continue;
     if(int(timeline.images[size_t(t)][q]>>24)==0)continue;
     if(pixelNearby(first,w,h,xx,yy,2)||pixelNearby(last,w,h,xx,yy,2))continue;
     timeline.images[size_t(t)][q]=0;stats.removedPixels++;
    }
   }
  }
  // A second guard handles 1-2 pixel splats that stay CONNECTED to the
  // silhouette (and therefore are invisible to component-track rejection).
  // Only activate for well-anchored/native-similar sprite pairs: unrelated
  // sprites and large unmodelled translations must never be clipped this way.
  int nativeAcount=0,nativeBcount=0,nativeCommon=0;
  for(size_t k=0;k<pixels;k++){
   bool aa=(first[k]>>24)>=112,bb=(last[k]>>24)>=112;
   nativeAcount+=aa;nativeBcount+=bb;nativeCommon+=(aa&&bb);
  }
  const int nativeUnion=nativeAcount+nativeBcount-nativeCommon;
  const bool confidentNativeMask=!fit.valid&&nativeUnion>0&&
    nativeCommon*100>=nativeUnion*60;
  if(confidentNativeMask){
   for(int time=1;time<n-1;time++){
    const double phase=double(time)/double(n-1);
    motion::Image pa,pb;
    if(fit.valid){
     pa=rotation::warp(first,w,h,fit.angle*phase,fit.dx*phase,fit.dy*phase,1+(fit.scale-1)*phase);
     pb=rotation::reverseWarp(last,w,h,fit,phase);
    }
    const auto& targetA=fit.valid?pa:first;
    const auto& targetB=fit.valid?pb:last;
    auto& image=timeline.images[size_t(time)];
    // The 1px native envelope tolerates rasterisation and antialiasing, but
    // not a floating fragment 2-3 pixels away from both original silhouettes.
    for(int id=0;id<int(pixels);id++){
     if(int(image[size_t(id)]>>24)<40)continue;
     int x=id%w,y=id/w;
     if(pixelNearby(targetA,w,h,x,y,1)||pixelNearby(targetB,w,h,x,y,1))continue;
     image[size_t(id)]=0;
     stats.removedPixels++;
    }
   }
  }
 }
 // Preserve native endpoints bit-for-bit. A centroid impulse on an otherwise
 // stable tiny shape is corrected only if the previous/next frames agree.
 for(int i=1;i<n-1;i++){
  Center a=center(timeline.images[i-1],w,h),b=center(timeline.images[i],w,h),c=center(timeline.images[i+1],w,h);
  if(a.weight<255*12||b.weight<255*12||c.weight<255*12)continue;
  double ex=(a.x+c.x)*.5,ey=(a.y+c.y)*.5;
  double deviation=std::hypot(b.x-ex,b.y-ey);
  stats.worstJitter=(std::max)(stats.worstJitter,deviation);
  if(deviation<2.5)continue;
  stats.suspiciousDrift++;
  if(std::hypot(a.x-c.x,a.y-c.y)>1.6||deviation<3.5)continue;
  int dx=std::clamp(int(std::lround(ex-b.x)),-2,2);
  int dy=std::clamp(int(std::lround(ey-b.y)),-2,2);
  if(!dx&&!dy)continue;
  motion::Image fixed(pixels,0);
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){
   int xx=x+dx,yy=y+dy;
   if(xx>=0&&xx<w&&yy>=0&&yy<h)fixed[size_t(yy)*w+xx]=timeline.images[i][size_t(y)*w+x];
  }
  timeline.images[i]=std::move(fixed);stats.correctedFrames++;
 }
 return stats;
}
} // namespace quality
