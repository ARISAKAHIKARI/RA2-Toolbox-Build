#include "type_hotkey.h"
static uint8_t csMask[MaxTypes];static uint32_t csMaskMatch;static long csMaskSeq=-1;
static void restoreCSHook(){csMaskSeq=-1;csMaskMatch=0;}
static bool csAllowedFor(uint32_t p,bool held){
 if(held&&(sh->flags&CustomCS))return rawArmed(p);
 if(!(sh->flags&AutoCS))return false;
 long seq=sh->csSeq;if(seq&1)return false;MemoryBarrier();if(sh->csMatch!=match)return false;
 if(seq!=csMaskSeq||csMaskMatch!=match){uint8_t next[MaxTypes];memcpy(next,sh->csAllowed,sizeof next);MemoryBarrier();if(seq!=sh->csSeq)return false;
  memcpy(csMask,next,sizeof csMask);csMaskSeq=seq;csMaskMatch=match;
 }
 uint32_t type=typeOf(p,kind(p));for(int i=0;i<snap.typeCount;++i)if(typePointers[i]==type)return csMask[i]&&rawArmed(p);
 return false;
}
static void csHooksTick(uint32_t flags){snap.csHookStatus=(flags&(AutoCS|CustomCS))?1:0;}
