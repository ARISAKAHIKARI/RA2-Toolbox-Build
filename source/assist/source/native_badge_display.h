#pragma once
#include "native_cameos.h"
#include "display_sample_lease.h"
namespace assist { namespace construction {
struct NativeDisplayKey {
 uint32_t pid=0,match=0,house=0;int width=0,height=0,tab=-1;
 bool operator==(const NativeDisplayKey&o)const{return pid==o.pid&&match==o.match&&house==o.house&&width==o.width&&height==o.height&&tab==o.tab;}
};
// This cache is only for passive badges. Native pointer/click validation calls
// readNativeView/readNative directly and cannot use these retained rectangles.
class NativeDisplay {
 displaylease::Sample<NativeDisplayKey,NativeFrame> lease_;
 NativeDisplayKey key_{};uint32_t attempted_=0;bool sampled_=false;
public:
 void clear(){lease_.clear();sampled_=false;key_={};}
 int tab()const{return key_.tab;}
 template<class Read>const NativeFrame* sample(Read read,NativeDisplayKey key,uint32_t now){
  key.tab=key_.tab;
  if(sampled_&&key==key_&&uint32_t(now-attempted_)<16)return lease_.get(key,now);
  attempted_=now;sampled_=true;
  NativeView view;NativeIdentity identity;auto state=sampleNativeView(read,view,identity);
  if(identity.tab>=0)key.tab=identity.tab;
  key_=key;
  if(state==NativeRead::Hidden||(identity.width&&identity.width!=key.width)||(identity.height&&identity.height!=key.height)){
   lease_.clear();return nullptr;
  }
  NativeFrame frame;if(state==NativeRead::Ready){frame.tab=view.tab;frame.top=view.top;frame.width=view.width;frame.height=view.height;frame.cells.assign(view.cells,view.cells+view.count);}
  return lease_.observe(key,state==NativeRead::Ready,std::move(frame),now);
 }
};
}}
