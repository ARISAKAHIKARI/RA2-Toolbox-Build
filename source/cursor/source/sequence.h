#pragma once
#include <stdint.h>
namespace cursorseq {
// Shared LONG fields carry unsigned serial bits. Zero is the initial sentinel;
// odd values denote a writer in progress. Ordering is modulo 2^32, within a
// half-range; a disconnected session must clear its consumer latch.
inline bool committed(uint32_t seq){return seq!=0&&!(seq&1u);}
inline bool currentOrNewer(uint32_t seq,uint32_t previous){return uint32_t(seq-previous)<0x80000000u;}
inline uint32_t next(uint32_t seq){uint32_t n=(seq+2u)&~1u;return n?n:2u;}
}
