#pragma once
#include "world_effects.h"
#include <array>
#include <memory>
namespace external { namespace gpueffects {
// Vera20K's ordered effect batches: CPU emits immutable geometry, GPU owns
// destination reads, blending and final pixels. Wave batches read a completed
// preceding destination snapshot. No graphics API is called by this planner.
// GPL-3.0 reference: terrain_surface_lines.rs/wgsl @ 3d42426b2e9c.
constexpr uint32_t Words=32,MaximumCommands=65536,MaximumReferences=2*1024*1024;
enum Kind:uint32_t {Line=0,Glow=1,Alpha=2,Wave=3,Bolt=4};
struct Command {std::array<uint32_t,Words> words{};std::shared_ptr<const std::vector<uint8_t>> indices;};
struct Batch {size_t first=0,count=0;bool snapshot=false;};
struct Plan {int width=0,height=0;std::vector<Command>commands;std::vector<Batch>batches;bool valid=true;};
struct Dispatch {std::array<uint32_t,12>params{};bool snapshot=false;std::array<uint32_t,4>readBounds{};};
inline float real(uint32_t n){float f;std::memcpy(&f,&n,4);return f;}
// Include maximum displaced reads, rounding, and destination writes. Two
// waves can share a snapshot only if these conservative footprints are disjoint.
inline std::array<uint32_t,4> footprint(const Command&c,int w,int h){
 const auto&a=c.words;int dx=0,dy=0;if(a[0]==Wave){dx=int(std::ceil(std::min(8192.,3.*std::abs(double(real(a[23]))))))+1;dy=int(std::ceil(std::min(8192.,3.*std::abs(double(real(a[24]))))))+1;}
 return {{uint32_t(std::max(0,int(a[4])-dx)),uint32_t(std::max(0,int(a[5])-dy)),uint32_t(std::min(w,int(a[6])+dx)),uint32_t(std::min(h,int(a[7])+dy))}};
}
struct CopySpan {uint64_t source=0,destination=0,bytes=0;};
inline std::vector<CopySpan> snapshotCopies(const Dispatch&d){
 std::vector<CopySpan>out;const auto&r=d.readBounds;uint64_t width=d.params[0],base=width*d.params[1]*4;
 for(uint32_t y=r[1];y<r[3];++y){uint64_t offset=(uint64_t(y)*width+r[0])*4,bytes=uint64_t(r[2]-r[0])*4;
  if(!out.empty()&&out.back().source+out.back().bytes==offset)out.back().bytes+=bytes;else out.push_back({offset,base+offset,bytes});}
 return out;
}
inline bool same(const std::shared_ptr<const Plan>&a,const std::shared_ptr<const Plan>&b){
 if(a==b)return true;if(!a||!b||!a->valid||!b->valid||a->width!=b->width||a->height!=b->height||a->commands.size()!=b->commands.size()||a->batches.size()!=b->batches.size())return false;
 for(size_t n=0;n<a->commands.size();++n)if(a->commands[n].words!=b->commands[n].words||a->commands[n].indices!=b->commands[n].indices)return false;
 for(size_t n=0;n<a->batches.size();++n){const auto&x=a->batches[n];const auto&y=b->batches[n];if(x.first!=y.first||x.count!=y.count||x.snapshot!=y.snapshot)return false;}return true;
}
// Vera terrain_batch dependency waves, adapted to our immutable wave commands.
// Normal color batches and bolt accumulation are hard ordering boundaries.
inline void scheduleWaves(Plan&p){
 std::vector<Command>ordered;std::vector<Batch>batches;ordered.reserve(p.commands.size());uint64_t visits=0;
 const int cols=(p.width+31)/32,rows=(p.height+31)/32;
 for(size_t at=0;at<p.batches.size();){const auto&batch=p.batches[at];
  if(!batch.snapshot){size_t first=ordered.size();for(size_t n=batch.first;n<batch.first+batch.count;++n)ordered.push_back(std::move(p.commands[n]));batches.push_back({first,batch.count,false});++at;continue;}
  size_t end=at;while(end<p.batches.size()&&p.batches[end].snapshot)++end;
  std::vector<int>last(size_t(cols)*rows,-1);std::vector<std::vector<size_t>>levels;
  for(size_t i=at;i<end;++i)for(size_t n=p.batches[i].first;n<p.batches[i].first+p.batches[i].count;++n){const auto&c=p.commands[n];if(c.words[0]!=Wave){p.valid=false;return;}auto r=footprint(c,p.width,p.height);int level=0;
   for(uint32_t y=r[1]/32;y<=(r[3]-1)/32;++y)for(uint32_t x=r[0]/32;x<=(r[2]-1)/32;++x){if(++visits>MaximumReferences){p.valid=false;return;}level=std::max(level,last[size_t(y)*cols+x]+1);}
   for(uint32_t y=r[1]/32;y<=(r[3]-1)/32;++y)for(uint32_t x=r[0]/32;x<=(r[2]-1)/32;++x)last[size_t(y)*cols+x]=level;
   if(levels.size()<=size_t(level))levels.resize(size_t(level)+1);levels[level].push_back(n);
  }
  for(auto&level:levels){size_t first=ordered.size();for(auto n:level)ordered.push_back(std::move(p.commands[n]));batches.push_back({first,level.size(),true});}at=end;
 }
 p.commands=std::move(ordered);p.batches=std::move(batches);
}
// Append to the scene's existing command upload. Small effects dispatch only
// their bounding rectangle; ordered 32-pixel bins avoid a full draw-list scan.
inline bool pack(const Plan&p,const std::vector<uint32_t>&alphas,uint32_t fog,std::vector<uint32_t>&words,std::vector<Dispatch>&jobs){
 if(!p.valid||p.width<1||p.height<1||p.width>8192||p.height>8192||p.commands.size()>MaximumCommands||alphas.size()!=p.commands.size())return false;
 const uint32_t base=uint32_t(words.size()),cols=(p.width+31)/32,rows=(p.height+31)/32,bins=cols*rows;
 for(size_t n=0;n<p.commands.size();++n){auto c=p.commands[n].words;if(c[0]>Bolt||c[4]>=c[6]||c[5]>=c[7]||c[6]>uint32_t(p.width)||c[7]>uint32_t(p.height))return false;if(c[0]==Alpha)c[3]=alphas[n];words.insert(words.end(),c.begin(),c.end());}
 uint64_t totalRefs=0;size_t covered=0;
 for(const auto&batch:p.batches){if(batch.first!=covered||!batch.count||batch.first>p.commands.size()||batch.count>p.commands.size()-batch.first)return false;
  covered+=batch.count;std::array<uint32_t,4>readBounds{{uint32_t(p.width),uint32_t(p.height),0,0}};
  std::vector<uint32_t>counts(bins),starts(bins+1);uint32_t l=p.width,t=p.height,r=0,b=0;
  auto visit=[&](size_t n,auto fn){auto&c=p.commands[n].words;for(uint32_t y=c[5]/32;y<=(c[7]-1)/32;++y)for(uint32_t x=c[4]/32;x<=(c[6]-1)/32;++x)fn(y*cols+x);};
  for(size_t n=batch.first;n<batch.first+batch.count;++n){auto&c=p.commands[n].words;if((c[0]==Wave)!=batch.snapshot)return false;if(batch.snapshot){auto f=footprint(p.commands[n],p.width,p.height);readBounds[0]=std::min(readBounds[0],f[0]);readBounds[1]=std::min(readBounds[1],f[1]);readBounds[2]=std::max(readBounds[2],f[2]);readBounds[3]=std::max(readBounds[3],f[3]);}l=std::min(l,c[4]);t=std::min(t,c[5]);r=std::max(r,c[6]);b=std::max(b,c[7]);visit(n,[&](uint32_t i){++counts[i];});}
  uint32_t refs=0;for(uint32_t i=0;i<bins;++i){starts[i]=refs;if(uint64_t(refs)+counts[i]+totalRefs>MaximumReferences)return false;refs+=counts[i];}starts[bins]=refs;totalRefs+=refs;
  auto next=starts;std::vector<uint32_t>references(refs);for(size_t n=batch.first;n<batch.first+batch.count;++n)visit(n,[&](uint32_t i){references[next[i]++]=uint32_t(n);});
  uint32_t binOffset=uint32_t(words.size());for(uint32_t i=0;i<bins;++i){words.push_back(starts[i]);words.push_back(counts[i]);}uint32_t refOffset=uint32_t(words.size());words.insert(words.end(),references.begin(),references.end());
  jobs.push_back({{uint32_t(p.width),uint32_t(p.height),base,binOffset,refOffset,cols,fog,uint32_t(p.width*p.height),l,t,r,b},batch.snapshot,readBounds});
 }return covered==p.commands.size();
}
inline uint32_t bits(float f){uint32_t n;std::memcpy(&n,&f,4);return n;}
class Builder {
 worldfx::View v_;std::shared_ptr<Plan> plan_;size_t start_=0;
 bool append(Command c){if(!plan_->valid)return false;if(plan_->commands.size()>=MaximumCommands){plan_->valid=false;return false;}if(c.words[4]>=c.words[6]||c.words[5]>=c.words[7])return true;plan_->commands.push_back(std::move(c));return true;}
 bool bounds(Command&c,double l,double t,double r,double b){
  if(!std::isfinite(l)||!std::isfinite(t)||!std::isfinite(r)||!std::isfinite(b))return false;
  c.words[4]=uint32_t(std::clamp(std::floor(l),0.,double(v_.pixelsW)));c.words[5]=uint32_t(std::clamp(std::floor(t),0.,double(v_.pixelsH)));
  c.words[6]=uint32_t(std::clamp(std::ceil(r),0.,double(v_.pixelsW)));c.words[7]=uint32_t(std::clamp(std::ceil(b),0.,double(v_.pixelsH)));return c.words[4]<c.words[6]&&c.words[5]<c.words[7];
 }
 void flush(bool snapshot=false){if(plan_->commands.size()>start_)plan_->batches.push_back({start_,plan_->commands.size()-start_,snapshot});start_=plan_->commands.size();}
 static uint32_t color(uint32_t rgb,double strength){uint32_t c=0;for(int sh:{0,8,16})c|=uint32_t(std::clamp(((rgb>>sh)&255)*strength,0.,255.))<<sh;return c;}
public:
 explicit Builder(const worldfx::View&view):v_(view),plan_(std::make_shared<Plan>()){plan_->width=view.pixelsW;plan_->height=view.pixelsH;plan_->valid=view.pixelsW>0&&view.pixelsH>0&&view.pixelsW<=8192&&view.pixelsH<=8192&&std::isfinite(view.width)&&std::isfinite(view.height)&&view.width>0&&view.height>0;}
 void pixel(int x,int y,uint32_t c,double strength=1,bool add=true){line({double(x),double(y)},{double(x),double(y)},c,strength,add,true);}
 void line(std::array<double,2>a,std::array<double,2>b,uint32_t rgb,double strength=1,bool add=true,bool point=false){
  if((!rgb&&!point)||strength<=0)return;double dx=b[0]-a[0],dy=b[1]-a[1],lo=0,hi=1;
  auto clip=[&](double p,double q){if(p==0)return q>=0;double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};
  if(!clip(-dx,a[0])||!clip(dx,v_.pixelsW-1-a[0])||!clip(-dy,a[1])||!clip(dy,v_.pixelsH-1-a[1]))return;
  double ax=a[0]+lo*dx,ay=a[1]+lo*dy,bx=a[0]+hi*dx,by=a[1]+hi*dy;Command c;c.words[0]=Line;c.words[1]=color(rgb,strength);c.words[2]=add;
  if(!bounds(c,std::min(ax,bx)-1,std::min(ay,by)-1,std::max(ax,bx)+2,std::max(ay,by)+2))return;
  c.words[8]=bits(float(ax));c.words[9]=bits(float(ay));c.words[10]=bits(float(bx));c.words[11]=bits(float(by));c.words[12]=point?0:uint32_t(std::max(1,int(std::ceil(std::max(std::abs(bx-ax),std::abs(by-ay))))));append(std::move(c));
 }
 void glow(const worldfx::Glow&s){if(s.flags&1)return;auto p=v_.project(s.point);double rx=std::clamp(s.size*v_.pixelsW/v_.width,1.,1024.),ry=std::clamp(s.size*.5*v_.pixelsH/v_.height,1.,1024.);Command c;c.words[0]=Glow;c.words[1]=((s.flags&2)?0:0xFF0000)|((s.flags&4)?0:0xFF00)|((s.flags&8)?0:0xFF);
  if(!bounds(c,int(p[0]-rx),int(p[1]-ry),int(p[0]+rx)+1,int(p[1]+ry)+1))return;c.words[8]=bits(float(p[0]));c.words[9]=bits(float(p[1]));c.words[10]=bits(float(rx));c.words[11]=bits(float(ry));append(std::move(c));}
 template<class Image>void alpha(const worldfx::Alpha&s,const std::shared_ptr<const Image>&image){
  if(!image||image->width<1||image->height<1||image->indices.size()!=size_t(image->width)*image->height)return;auto p=v_.project(s.point);double sx=v_.pixelsW/v_.width,sy=v_.pixelsH/v_.height;
  int left=int(std::floor(p[0]+(s.dx+image->x+image->canvasWidth/2)*sx)),top=int(std::floor(p[1]+(s.dy+image->y+image->canvasHeight/2)*sy));Command c;c.words[0]=Alpha;
  if(!bounds(c,left,top,left+std::max(1,int(std::ceil(image->width*sx))),top+std::max(1,int(std::ceil(image->height*sy)))))return;
  c.words[8]=bits(float(left));c.words[9]=bits(float(top));c.words[10]=bits(float(sx));c.words[11]=bits(float(sy));c.words[14]=image->width;c.words[15]=image->height;c.indices=std::shared_ptr<const std::vector<uint8_t>>(image,&image->indices);append(std::move(c));
 }
 void wave(const worldfx::Wave&s){
  auto from=v_.project(s.from),to=v_.project(s.to);std::array<std::array<double,2>,4>p;for(int i=0;i<4;++i)p[i]=v_.project(s.corners[i]);auto lerp=[](auto a,auto b,double t){return std::array<double,2>{{a[0]+(b[0]-a[0])*t,a[1]+(b[1]-a[1])*t}};};std::vector<std::array<double,2>>poly;
  if(s.kind==3)poly={lerp(p[0],p[2],s.front),lerp(p[1],p[3],s.front),lerp(p[1],p[3],s.back),lerp(p[0],p[2],s.back)};
  else poly={lerp(p[0],p[2],s.front),lerp(from,to,s.front),lerp(p[1],p[3],s.front),lerp(p[1],p[3],s.back),lerp(from,to,s.back),lerp(p[0],p[2],s.back)};
  double l=poly[0][0],r=l,t=poly[0][1],b=t;Command c;c.words[0]=Wave;for(size_t i=0;i<poly.size();++i){l=std::min(l,poly[i][0]);r=std::max(r,poly[i][0]);t=std::min(t,poly[i][1]);b=std::max(b,poly[i][1]);c.words[8+i*2]=bits(float(poly[i][0]));c.words[9+i*2]=bits(float(poly[i][1]));}
  if(!bounds(c,l,t,r+1,b+1))return;c.words[20]=uint32_t(poly.size());c.words[21]=bits(float(from[0]));c.words[22]=bits(float(from[1]));c.words[23]=bits(float(v_.pixelsW/v_.width));c.words[24]=bits(float(v_.pixelsH/v_.height));c.words[25]=s.phase;c.words[26]=s.kind;c.words[27]=s.direction;flush();append(std::move(c));flush(true);
 }
 void paint(const worldfx::Snapshot&s,bool particles,bool lasers){worldfx::Geometry<Builder>(*this,v_).paint(s,particles,lasers);}
 // The native-feedback strand generator emits bounded line geometry only.
 // Its premultiplied layer is accumulated on the GPU and masked once, just
 // like the CPU reference; partial fog must not be multiplied per segment.
 void line(double x0,double y0,double x1,double y1,uint32_t color,int thickness=1){
  double dx=x1-x0,dy=y1-y0,lo=0,hi=1;auto clip=[&](double p,double q){if(p==0)return q>=0;double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};
  if(!std::isfinite(x0)||!std::isfinite(y0)||!std::isfinite(x1)||!std::isfinite(y1)||!clip(-dx,x0)||!clip(dx,v_.pixelsW-1-x0)||!clip(-dy,y0)||!clip(dy,v_.pixelsH-1-y0))return;
  int ax=int(std::lround(x0+lo*dx)),ay=int(std::lround(y0+lo*dy)),bx=int(std::lround(x0+hi*dx)),by=int(std::lround(y0+hi*dy)),radius=(std::clamp(thickness,1,9)-1)/2;Command c;c.words[0]=Bolt;c.words[1]=color;
  if(!bounds(c,std::min(ax,bx)-radius,std::min(ay,by)-radius,std::max(ax,bx)+radius+1,std::max(ay,by)+radius+1))return;
  c.words[8]=uint32_t(ax);c.words[9]=uint32_t(ay);c.words[10]=uint32_t(bx);c.words[11]=uint32_t(by);c.words[13]=radius;append(std::move(c));
 }
 void bolts(const std::vector<nativefeedback::Bolt>&list,int frame){flush();for(const auto&e:list){auto a=v_.project(e.source),b=v_.project(e.target);double scale=std::min(v_.pixelsW/v_.width,v_.pixelsH/v_.height),margin=8*scale+8;if(std::max(a[0],b[0])<-margin||std::min(a[0],b[0])>v_.pixelsW+margin||std::max(a[1],b[1])<-margin||std::min(a[1],b[1])>v_.pixelsH+margin)continue;nativefeedback::bolt(*this,a[0],a[1],b[0],b[1],e.seed^uint32_t(frame),e.alternate,scale);}flush();}
 std::shared_ptr<const Plan> finish(){flush();if(plan_->valid)scheduleWaves(*plan_);return plan_;}
};
}}
