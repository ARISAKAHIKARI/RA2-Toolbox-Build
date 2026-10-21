#pragma once
#include <cstdint>
#include <mutex>
// EXE-only presentation state shared by the Tactical and Cursor UI threads.
// No game write, input interception, command submission or extra Runtime IPC.
namespace commandcursor {
enum Mode : uint32_t { None, Advanced, AirFirst, AirSecond, CS, Magnetron, Enter, Disk };
struct Hint {uint32_t pid=0,match=0,tick=0,binding=0;Mode mode=None;};
struct Bus {std::mutex mutex;Hint hint;};
inline Bus& bus(){static Bus b;return b;}
inline void publish(Hint h){auto&b=bus();std::lock_guard<std::mutex> lock(b.mutex);b.hint=h;}
inline void clear(){publish({});}
inline Hint read(uint32_t pid,uint32_t now){auto&b=bus();std::lock_guard<std::mutex> lock(b.mutex);
 return b.hint.pid==pid&&uint32_t(now-b.hint.tick)<=100?b.hint:Hint{};}
// Only ordinary tactical cursors may be replaced. Scroll, sell, repair,
// superweapon targets, waypoints and other native tools keep their own art.
inline bool ordinary(int id){return id==0||(id>=17&&id<=21)||id==71;}
inline int definition(Mode m){return m==Advanced||m==CS||m==Magnetron?71:m==Enter?25:20;}
inline uint32_t recolor(uint32_t p,Mode m,int x,int y,int w,int h){
 unsigned a=p>>24,r=(p>>16)&255,g=(p>>8)&255,b=p&255;
 if(!a)return p;
 unsigned hi=r>g?r:g;hi=hi>b?hi:b;
 unsigned lo=r<g?r:g;lo=lo<b?lo:b;
 // Preserve alpha, shadows, outlines and neutral highlights.
 if(hi-lo<24||hi<40)return p;
 if(m==Advanced){
  if(g>r+20&&g>b+20){r=g;} // green -> yellow
  else if(r>b+24&&r>g+20){g=r;r=b;} // red -> green in stock attack-move art
  else if(r>b+24&&g>b+24&&r*2>=g&&g*2>=r){r=b;} // yellow -> green
 }else if(m==AirFirst||m==AirSecond){
  // Normalize against the entire SHA canvas, not the cropped frame. The
  // center stays fixed across animation frames and asymmetric crop bounds.
  const int dx=2*x-(w-1),dy=2*y-(h-1);
  bool inner=int64_t(dx)*dx*36<int64_t(w)*w*9&&int64_t(dy)*dy*36<int64_t(h)*h*9;
  inner=inner&&int64_t(dx)*dx*h*h+int64_t(dy)*dy*w*w<int64_t(w)*w*h*h/4;
  r=inner?0:hi;g=inner?hi:0;b=0;
 }else if(m==Magnetron){r=hi;g=hi/4;b=hi;}
 else if(m==Enter){r=0;g=hi;b=hi;}
 else if(m==Disk){r=hi;g=hi/2;b=0;}
 return (a<<24)|(r<<16)|(g<<8)|b;
}
}
