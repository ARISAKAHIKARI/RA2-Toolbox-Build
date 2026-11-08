#pragma once
// Only pure collector code calls this helper. Preserve a successfully observed
// empty engine vector; do not turn a failed header/items read into an empty list.
// Item bytes are decoded through the same scoped read/get path as the header.
static bool sampleRoster(uint32_t address,Vec&out,int limit=16384){
 out={};
 return read(address,&out,sizeof out)&&out.count>=0&&out.count<=limit&&out.capacity>=out.count&&
  (!out.count||(out.items>=0x10000&&uint64_t(out.items)+uint64_t(out.count)*4<0x80000000ull));
}
