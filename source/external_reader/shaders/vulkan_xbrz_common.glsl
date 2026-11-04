// GPU port of Zenju's xBRZ 1.9 ARGB-unbuffered 2x/3x/4x rules.
// Copyright (C) Zenju (zenju AT gmx DOT de), original xBRZ author.
// SPDX-License-Identifier: GPL-3.0-only
// This derived port retains the upstream rule coefficients and operation order.
// Original unmodified source/license: ../../third_party/xbrz/.
// Float64 and NoContraction retain double expressions and their order; GPU
// sqrt/division still need per-device oracle validation, not an all-input proof.
// Color gradients and alpha marshaling use exact unsigned arithmetic.
layout(set=0,binding=0,std430) readonly buffer NativeSource { uint pixels[]; } nativeSource;
layout(set=0,binding=1,std430) buffer CornerFlags { uint value[]; } cornerFlags;
layout(set=0,binding=2,std430) writeonly buffer Reconstructed { uint pixels[]; } reconstructed;
layout(push_constant) uniform XbrzParams {
 uint inputOffset,width,height,outputOffset,factor,flagsOffset;
} p;
uint divideRound(uint n,uint d){return (n+d/2u)/d;}
uint straight(uint pixel){
 uint a=pixel>>24;if(a==0u)return 0u;if(a==255u)return pixel;
 uint result=a<<24;
 for(uint shift=0u;shift<24u;shift+=8u)result|=min(255u,((((pixel>>shift)&255u)*255u+a/2u)/a))<<shift;
 return result;
}
uint premultiplied(uint pixel){
 uint a=pixel>>24,result=a<<24;
 for(uint shift=0u;shift<24u;shift+=8u)result|=((((pixel>>shift)&255u)*a+127u)/255u)<<shift;
 return result;
}
uint readSource(ivec2 at){
 if(at.x<0||at.y<0||at.x>=int(p.width)||at.y>=int(p.height))return 0u;
 return straight(nativeSource.pixels[p.inputOffset+uint(at.y)*p.width+uint(at.x)]);
}
double distanceARGB(uint left,uint right){
 precise double red=double(int((left>>16)&255u)-int((right>>16)&255u));
 precise double green=double(int((left>>8)&255u)-int((right>>8)&255u));
 precise double blue=double(int(left&255u)-int(right&255u));
 const double kb=0.0593LF,kr=0.2627LF,kg=1.0LF-kb-kr;
 const double sb=0.5LF/(1.0LF-kb),sr=0.5LF/(1.0LF-kr);
 precise double y=kr*red+kg*green+kb*blue;
 precise double cb=sb*(blue-y),cr=sr*(red-y);
 precise double magnitude=sqrt(y*y+cb*cb+cr*cr);
 precise double a1=double(left>>24)/255.0LF,a2=double(right>>24)/255.0LF;
 precise double result;
 if(a1<a2)result=a1*magnitude+255.0LF*(a2-a1);
 else result=a2*magnitude+255.0LF*(a1-a2);
 return result;
}
bool equalColor(uint a,uint b){return distanceARGB(a,b)<30.0LF;}
uint gradientARGB(uint front,uint back,uint m,uint n){
 uint wf=(front>>24)*m,wb=(back>>24)*(n-m),sum=wf+wb;
 if(sum==0u)return 0u;
 uint result=divideRound(sum,n)<<24;
 for(uint shift=0u;shift<24u;shift+=8u)result|=divideRound(((front>>shift)&255u)*wf+((back>>shift)&255u)*wb,sum)<<shift;
 return result;
}
