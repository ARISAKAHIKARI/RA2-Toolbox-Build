#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
namespace watchpresentation {
// Request timestamps and panel positions are not image identity. An immutable
// completed image can survive worker retries, exposures and a dragged HWND.
struct Visual {
 std::shared_ptr<const void> image;
 uint64_t sequence=0;int sourceWidth=0,sourceHeight=0,width=0,height=0;
 bool gpu=false,dangerous=false;
};
class Gate {
 std::weak_ptr<const void> image_;uint64_t sequence_=0;
 int sourceWidth_=0,sourceHeight_=0,width_=0,height_=0;bool gpu_=false,dangerous_=false,known_=false;
public:
 void invalidate(){image_.reset();known_=false;}
 bool needs(const Visual&v)const{return v.image&&(!known_||image_.lock()!=v.image||sequence_!=v.sequence||sourceWidth_!=v.sourceWidth||sourceHeight_!=v.sourceHeight||width_!=v.width||height_!=v.height||gpu_!=v.gpu||dangerous_!=v.dangerous);}
 void committed(const Visual&v){image_=v.image;sequence_=v.sequence;sourceWidth_=v.sourceWidth;sourceHeight_=v.sourceHeight;width_=v.width;height_=v.height;gpu_=v.gpu;dangerous_=v.dangerous;known_=true;}
};
inline double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// GetTickCount can advance between the caller's timestamp and submission;
// clamp a future stamp instead of converting it to a 49-day queue delay.
inline uint32_t queueDelay(uint32_t now,uint32_t submitted){const int32_t elapsed=int32_t(now-submitted);return elapsed>0?uint32_t(elapsed):0;}
struct Metric {
 uint64_t count=0,failed=0;double total=0,maximum=0;
 void add(double ms,bool success=true){++count;failed+=!success;total+=ms;if(ms>maximum)maximum=ms;}
 double average()const{return count?total/count:0;}
};
}
