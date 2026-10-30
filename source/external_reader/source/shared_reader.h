#pragma once
#include "process_reader.h"
namespace external {
// Unified external read API: independent task caches, shared connection.
using ReadSession=ProcessReader;
using SharedProcessReader=ReadSession; // compatibility for external consumers
struct SharedReadView {ReadSession*reader;bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}bool fresh(uint32_t p,void*out,size_t n)const{return reader->readFresh(p,out,n);}void prefetch(const std::vector<uint32_t>&addresses,size_t span)const{reader->prefetch(addresses,span);}};
}
