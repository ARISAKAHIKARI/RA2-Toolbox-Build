#pragma once
#include <stddef.h>
#include <stdint.h>
// Strict subset of x86 *non-branching* instructions. Unknown instructions,
// branch/call instructions, old detours and relative operands are rejected.
// This parser is intentionally NOT a general x86 decoder.
namespace frameprologue {
inline unsigned safeInstructionLength(const uint8_t* p, unsigned available){
 if(!p||!available)return 0;
 const uint8_t op=p[0];
 if((op>=0x50&&op<=0x5f)||op==0x90||op==0x9c||op==0x9d||op==0xfc)return 1;
 if(op==0x6a)return available>=2?2:0;
 if(op==0x68||(op>=0xa0&&op<=0xa3)||(op>=0xb8&&op<=0xbf))return available>=5?5:0;
 if(op==0x8b||op==0x89||op==0x8d||op==0x85||op==0x33||op==0x31||
    op==0x83||op==0x81||op==0x80||op==0xc7||op==0x8a||op==0x88){
  if(available<2)return 0;
  uint8_t modrm=p[1],mod=modrm>>6,rm=modrm&7;
  unsigned length=2;
  if(mod!=3&&rm==4){if(available<3)return 0;uint8_t sib=p[2];++length;
   if(mod==0&&(sib&7)==5)length+=4;
  }else if(mod==0&&rm==5)length+=4;
  if(mod==1)++length;else if(mod==2)length+=4;
  if(op==0x83||op==0x80)++length;
  if(op==0x81||op==0xc7)length+=4;
  // C7 /0 is MOV imm32; other C7 forms have special semantics.
  if(op==0xc7&&((modrm>>3)&7)!=0)return 0;
  return length<=available?length:0;
 }
 return 0;
}
inline unsigned safeOverwrite(const uint8_t* bytes,unsigned available){
 if(!bytes||available<5)return 0;
 // Do not chain/overwrite a jump, breakpoint, return or unsupported prefix.
 if(bytes[0]==0xe9||bytes[0]==0xeb||bytes[0]==0xe8||bytes[0]==0xcc||
    bytes[0]==0xc3||bytes[0]==0xff||bytes[0]==0x0f)return 0;
 unsigned n=0;
 while(n<5){unsigned step=safeInstructionLength(bytes+n,available-n);
  if(!step||n+step>12)return 0;n+=step;
 }
 return n;
}
}
